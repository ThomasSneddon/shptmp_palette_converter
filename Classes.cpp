#include "Classes.h"

#include <cassert>

CLASSES_START

tmpfile::tmpfile(std::string filename) :tmpfile()
{
    load(filename);
}

void tmpfile::clear()
{
    _imageheaders.clear();
    _original_offsets.clear();
}

bool tmpfile::is_loaded()
{
    return !_imageheaders.empty() && !_original_offsets.empty();
}

bool tmpfile::load(std::string filename)
{
    clear();

    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file)
        return false;

    std::vector<byte> buffer;

    file.seekg(0, std::ios::end);
    size_t filesize = file.tellg();

    buffer.resize(filesize);
    file.seekg(0, std::ios::beg);

    file.read(reinterpret_cast<char*>(buffer.data()), filesize);

    tmp_image_header temp;

    memcpy_s(&_fileheader, sizeof _fileheader, buffer.data(), sizeof _fileheader);
    _original_offsets.resize(block_count());

    size_t current_valid_index = 0;
    memcpy_s(_original_offsets.data(), block_count() * sizeof uint32_t, &buffer[sizeof _fileheader], block_count() * sizeof uint32_t);
    for (size_t offset : _original_offsets)
    {
        const size_t header_size = sizeof tmp_image_header - sizeof std::vector<byte>;
        if (offset)
        {
            //offset += reinterpret_cast<uint32_t>(buffer.data());
            memcpy_s(&temp, header_size, &buffer[offset], header_size);

            temp.pixels.resize(tile_size() * 2);
            memcpy_s(temp.pixels.data(), tile_size() * 2, &buffer[offset + header_size], tile_size() * 2);

            _imageheaders.push_back(temp);
            if (has_extra(current_valid_index))
            {
                auto& back = _imageheaders.back();
                back.pixels.resize(tile_size() * 2 + extra_size(current_valid_index) * 2);
                memcpy_s(&back.pixels[tile_size() * 2], extra_size(current_valid_index) * 2, &buffer[offset + header_size + tile_size() * 2], extra_size(current_valid_index) * 2);
            }

            ++current_valid_index;
        }
    }

    file.close();
    return true;
}

size_t tmpfile::block_count()
{
    return _fileheader.xblocks* _fileheader.yblocks;
}

size_t tmpfile::valid_block_count()
{
    return _imageheaders.size();
}

size_t tmpfile::tile_size()
{
    return _fileheader.block_height * _fileheader.block_width / 2;
}

byte* tmpfile::color_data(size_t index)
{
    if (index >= _imageheaders.size())
        return nullptr;
    return _imageheaders[index].pixels.data();
}

byte* tmpfile::zbuffer_data(size_t index)
{
    if (index >= _imageheaders.size())
        return nullptr;
    return &_imageheaders[index].pixels[tile_size()];
}

bool tmpfile::has_extra(size_t index)
{
    if (index >= _imageheaders.size())
        return false;
    return _imageheaders[index].ex_flags & 1u;
}

byte* tmpfile::extra_data(size_t index)
{
    if (!has_extra(index))
        return nullptr;
    return &_imageheaders[index].pixels[tile_size() * 2];
}

byte* tmpfile::extra_zbuffer(size_t index)
{
    if (!has_extra(index))
        return nullptr;
    return &_imageheaders[index].pixels[tile_size() * 2 + extra_size(index)];
}

size_t tmpfile::extra_size(size_t index)
{
    if (!has_extra(index))
        return 0;
    return _imageheaders[index].ex_width * _imageheaders[index].ex_height;
}

bool tmpfile::color_replace(std::vector<byte> replace_scheme)
{
    const size_t valid_color_count = 256;
    if (!is_loaded() || replace_scheme.size() != valid_color_count)
        return false;

    for (size_t i = 0; i < valid_block_count(); i++)
    {
        byte* colors = color_data(i);
        for (size_t x = 0; x < tile_size(); x++)
            colors[x] = replace_scheme[colors[x]];

        if (byte* extras = extra_data(i))
        {
            for (size_t x = 0; x < extra_size(i); x++)
                extras[x] = replace_scheme[extras[x]];
        }
    }
    
    return true;
}

size_t tmpfile::calculate_file_size()
{
    if (!is_loaded())
        return 0;

    const size_t header_size = sizeof tmp_image_header - sizeof std::vector<byte>;
    size_t total_size = sizeof _fileheader + block_count() * sizeof uint32_t;
    for (auto& block_data : _imageheaders)
        total_size += header_size + block_data.pixels.size();

    return total_size;
}

bool tmpfile::save(std::string filename)
{
    if (!is_loaded())
        return false;

    std::vector<byte> filebuffer;
    std::ofstream file(filename, std::ios::out | std::ios::binary);
    if (!file)
        return false;

    filebuffer.resize(calculate_file_size());
    const size_t image_header_size = sizeof tmp_image_header - sizeof std::vector<byte>;

    memcpy_s(filebuffer.data(), sizeof _fileheader, &_fileheader, sizeof _fileheader);
    memset(&filebuffer[sizeof _fileheader], 0, block_count() * sizeof uint32_t);
    
    uint32_t* offsets = reinterpret_cast<uint32_t*>(&filebuffer[sizeof _fileheader]);
    uint32_t current_offset = sizeof _fileheader + block_count() * sizeof uint32_t;
    size_t block_data_size = image_header_size + tile_size() * 2;

    //memset(offsets, 0, block_count() * sizeof uint32_t);
    memcpy_s(offsets, block_count() * sizeof uint32_t, _original_offsets.data(), block_count() * sizeof uint32_t);
    for (size_t i = 0; i < valid_block_count(); i++)
    {
        auto& block_data = _imageheaders[i];
        memcpy_s(&filebuffer[current_offset], image_header_size, &block_data, image_header_size);
        memcpy_s(&filebuffer[current_offset + image_header_size], block_data.pixels.size(), block_data.pixels.data(), block_data.pixels.size());

        //offsets[i] = current_offset;
        current_offset += image_header_size + block_data.pixels.size();
    }

    file.write(reinterpret_cast<char*>(filebuffer.data()), filebuffer.size());
    file.close();
    return true;
}

palette::palette(std::string filename)
{
    load(filename);
}

bool palette::load(std::string filename)
{
    const size_t valid_color_count = 256;
    constexpr size_t valid_palsize = valid_color_count * sizeof color;

    clear();

    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file)
        return false;

    size_t filesize = file.seekg(0, std::ios::end).tellg();
    file.seekg(0, std::ios::beg);
    if (filesize != valid_palsize)
        return false;

    _entries.resize(valid_color_count);
    file.read(reinterpret_cast<char*>(_entries.data()), valid_palsize);
    for (auto& color : _entries)
    {
        color.r <<= 2;
        color.g <<= 2;
        color.b <<= 2;
    }

    file.close();
    return true;
}

void palette::clear()
{
    _entries.clear();
}

bool palette::is_loaded()
{
    return !_entries.empty();
}

std::vector<byte> palette::convert_color(palette& target)
{
    const size_t valid_color_count = 256;
    std::vector<byte> convert_table;

    if (!is_loaded() || !target.is_loaded())
        return convert_table;

    convert_table.resize(valid_color_count);
    auto distance_squared = [](color& left, color& right)->size_t
    {
        size_t dis_r = right.r - left.r;
        size_t dis_g = right.g - left.g;
        size_t dis_b = right.b - left.b;
        return dis_r * dis_r + dis_g * dis_g + dis_b * dis_b;
    };

    auto find_nearest_color = [valid_color_count, &target, &distance_squared](color& color)->size_t
    {
        size_t nearest_index = 0;
        size_t nearest_distance = distance_squared(color, target[0]);
        for (size_t i = 1; i < valid_color_count; i++)
        {
            size_t distance = distance_squared(color, target[i]);
            if (distance < nearest_distance)
            {
                nearest_distance = distance;
                nearest_index = i;
            }
        }

        return nearest_index;
    };

    for (size_t i = 1; i < valid_color_count; i++)
        convert_table[i] = find_nearest_color(_entries[i]);

    convert_table[0] = 0;
    return convert_table;
}

color& palette::operator[](size_t index)
{
    return _entries[index];
}

shpfile::shpfile(std::string filename) :shpfile()
{
    load(filename);
}

void shpfile::clear()
{
    _frameheaders.clear();
    _pixels.clear();
}

bool shpfile::is_loaded()
{
    return !_frameheaders.empty() && !_pixels.empty();
}

bool shpfile::load(std::string filename)
{
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file)
        return false;

    clear();

    std::vector<byte> buffer;
    
    size_t filesize = file.seekg(0, std::ios::end).tellg();
    file.seekg(0, std::ios::beg);
    buffer.resize(filesize);

    file.read(reinterpret_cast<char*>(buffer.data()), filesize);
    memcpy_s(&_fileheader, sizeof _fileheader, buffer.data(), sizeof _fileheader);
    
    _frameheaders.resize(frame_count());
    memcpy_s(_frameheaders.data(), frame_count() * sizeof shp_frame_header, &buffer[sizeof _fileheader], frame_count() * sizeof shp_frame_header);

    uint32_t pixels_offset = sizeof _fileheader + frame_count() * sizeof shp_frame_header;
    size_t pixels_size = buffer.size() - pixels_offset;

    _pixels.resize(pixels_size);
    memcpy_s(_pixels.data(), pixels_size, &buffer[pixels_offset], pixels_size);

    file.close();
    return true;
}

size_t shpfile::frame_count()
{
    return _fileheader.frames;
}

byte* shpfile::pixel_data(size_t index)
{
    if (index >= _frameheaders.size())
        return nullptr;

    shp_frame_header& header = _frameheaders[index];
    uint32_t offset = header.data_offset - sizeof _fileheader - _frameheaders.size() * sizeof shp_frame_header;
    
    return &_pixels[offset];
}

rectangle shpfile::frame_bound(size_t index)
{
    rectangle temp{ 0,0,0,0 };

    if (index >= _frameheaders.size())
        return temp;

    shp_frame_header& frame_data = _frameheaders[index];
    temp = { frame_data.x,frame_data.y,frame_data.width,frame_data.height };
    
    return temp;
}

rectangle shpfile::file_bound()
{
    return rectangle{ 0,0,_fileheader.width,_fileheader.height };
}

bool shpfile::color_replace(std::vector<byte> replace_scheme)
{
    const size_t valid_replace_count = 256;

    if (!is_loaded() || replace_scheme.size() != valid_replace_count)
        return false;

    for (size_t i = 0; i < frame_count(); i++)
    {
        byte* colors = pixel_data(i);
        shp_frame_header& header = _frameheaders[i];
        if (!colors)
            continue;

        size_t width = header.width;
        size_t height = header.height;
        bool has_compression = header.flags & 2u;

        if (has_compression)
        {
            for (size_t l = 0; l < height; l++)
            {
                byte* current = colors + sizeof uint16_t;
                uint16_t pitch = *reinterpret_cast<uint16_t*>(colors);

                while (current != colors + pitch)
                {
                    if (*current)
                    {
                        *current = replace_scheme[*current];
                        current++;
                    }
                    else
                    {
                        current += 2;
                    }
                }
                colors += pitch;
            }
        }
        else
        {
            for (size_t l = 0; l < height; l++)
            {
                for (size_t x = 0; x < width; x++)
                    colors[l * width + x] = replace_scheme[colors[l * width + x]];
            }
        }
    }
    return true;
}

size_t shpfile::calculate_file_size()
{
    if (!is_loaded())
        return 0;
    return sizeof _fileheader + frame_count() * sizeof shp_frame_header + _pixels.size();
}

bool shpfile::save(std::string filename)
{
    if (!is_loaded())
        return false;

    std::ofstream file(filename, std::ios::out | std::ios::binary);
    if (!file)
        return false;

    file.write(reinterpret_cast<char*>(&_fileheader), sizeof _fileheader);
    file.write(reinterpret_cast<char*>(_frameheaders.data()), _frameheaders.size() * sizeof shp_frame_header);
    file.write(reinterpret_cast<char*>(_pixels.data()), _pixels.size());

    file.close();
    return true;
}


void config::trim(std::string& string, const char* filter)
{
    string.erase(0, string.find_first_not_of(filter));
    string.erase(string.find_last_not_of(filter) + 1);//std::string::npos + 1 == 0;
}

void config::remove_annotation(std::string& string)
{
    if (string.find(';') != string.npos)
        string.erase(string.find(';'));

    if (string.find("//") != string.npos)
        string.erase(string.find("//"));
}

std::string config::split(std::string& string, char delim)
{
    std::string right;

    if (string.find(delim) != string.npos)
    {
        right = string;
        string.erase(string.find(delim));
        trim(string);

        right.erase(0, right.find(delim) + 1);
        trim(right);
    }

    return right;
}

config::value_type config::split_values(std::string string)
{
    std::vector<std::string> values;
    std::string current;
    size_t current_off;
    size_t string_off;

    string_off = 0;
    current_off = string.find(',');
    current.assign(string, string_off, current_off);
    trim(current);

    while (!current.empty())
    {
        values.push_back(current);
        if (current_off == std::string::npos)
            break;

        string_off = current_off + 1;
        current_off = string.find(',', current_off + 1);
        current.assign(string, string_off, current_off - string_off);
        trim(current);
    }

    return values;
}

bool config::is_section(const std::string& string)
{
    std::string copy = string;
    
    trim(copy);
    return !copy.empty() && copy.front() == '[' && copy.back() == ']' && copy.find(';') == std::string::npos;
}

bool config::is_empty(const std::string string)
{
    std::string copy = string;
    
    trim(copy);
    return copy.empty();
}

void config::convert_section_name(std::string& string)
{
    trim(string," \t\r\n;[]");
}

bool config::is_annotation(const std::string& string)
{
    std::string copy = string;
    trim(copy, " \t\r\n");

    return !copy.empty() && (copy.front() == ';' || copy.substr(0, 2) == "//");
}

config::config(std::string filename)
{
    load(filename);
}

bool config::load(std::string filename)
{
    char linebuff[0x200]{ 0 };
    std::ifstream file(filename);

    if (!file)
        return false;

    std::string current_section;
    while (file.getline(linebuff, sizeof linebuff))
    {
        if (is_annotation(linebuff) || is_empty(linebuff))
            continue;

        std::string key = linebuff;
        remove_annotation(key);

        if (is_section(key))
        {
            current_section = key;
            convert_section_name(current_section);
            continue;
        }
        
        std::string right = split(key);
        value_type values = split_values(right);

        if (values.empty() || key.empty())
            continue;

        _config[current_section][key] = values;
    }

    return is_loaded();
}

bool config::is_loaded()
{
    return !_config.empty();
}

void config::clear()
{
    _config.clear();
}

config::section_type config::operator[](std::string name)
{
    return section(name);
}

config::section_type config::section(std::string name)
{
    section_type section;
    
    config_type::const_iterator iter = _config.find(name);
    if (iter == _config.end())
        return section;
    return iter->second;
}

config::value_type config::value(std::string secname, std::string key)
{
    value_type values;
    section_type sec = section(secname);

    if (sec.empty())
        return values;

    section_type::const_iterator iter = sec.find(key);
    if (iter == sec.end())
        return values;
    return iter->second;
}

std::vector<int> config::value_as_int(std::string section, std::string key)
{
    std::vector<int> ret;
    value_type values = value(section, key);

    ret.resize(values.size());
    for (size_t i = 0; i < ret.size(); i++)
    {
        ret[i] = atoi(values[i].c_str());
    }

    return ret;
}

std::vector<bool> config::value_as_bool(std::string section, std::string key, bool def)
{
    std::vector<bool> ret;
    value_type values = value(section, key);

    for (std::string value : values)
    {
        char first = std::toupper(value.front());
        if (first == 'T' || first == 'Y' || first == '1')
            ret.push_back(true);
        else if (first == 'F' || first == 'N' || first == '0')
            ret.push_back(false);
        else
            ret.push_back(def);
    }

    return ret;
}

int config::read_int(std::string section, std::string key, int def)
{
    std::vector<int> values = value_as_int(section, key);
    if (values.empty())
        return def;
    return values.front();
}

bool config::read_bool(std::string section, std::string key, bool def)
{
    std::vector<bool> values = value_as_bool(section, key, def);
    if (values.empty())
        return def;
    return values.front();
}

std::string config::read_string(std::string section, std::string key, std::string def)
{
    value_type values = value(section, key);
    if (values.empty())
        return def;
    return values.front();
}

CLASSES_END