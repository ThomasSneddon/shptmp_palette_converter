#pragma once
//baisc
#include <Windows.h>

//traits
#include <type_traits>

//streams
#include <fstream>
#include <iostream>

//containers
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#define CLASSES_START namespace thomas{
#define CLASSES_END };

CLASSES_START

enum ramp :char
{
	Plane,
	NW, NE, SE, SW,
	N, E, S, W,
	NH, EH, SH, WH,
	DmN, DmE, DmS, DmW,
	DnWE, UpWE, DnNS, UpNS
};

struct tmp_file_header
{
	size_t xblocks;
	size_t yblocks;
	size_t block_width;
	size_t block_height;
};

struct tmp_image_header
{
	int32_t x;
	int32_t y;
	uint32_t _reserved_1[3];
	int32_t x_extra;
	int32_t y_extra;
	size_t ex_width;
	size_t ex_height;
	uint32_t ex_flags;
	byte height;
	byte type;
	ramp ramp_type;
	byte _reserved_2[9];
	std::vector<byte> pixels;
};

struct color
{
	byte r;
	byte g;
	byte b;
};

class tmpfile
{
public:
	tmpfile() = default;
	tmpfile(std::string filename);
	~tmpfile() = default;

	//load and clear
	void clear();
	bool is_loaded();
	bool load(std::string filename);

	//data accessing
	size_t block_count();
	size_t valid_block_count();
	size_t tile_size();
	byte* color_data(size_t index);
	byte* zbuffer_data(size_t index);
	bool has_extra(size_t index);
	byte* extra_data(size_t index);
	byte* extra_zbuffer(size_t index);
	size_t extra_size(size_t index);

	//data modifier
	bool color_replace(std::vector<byte> replace_scheme);

	//save 
	size_t calculate_file_size();
	bool save(std::string filename);

private:
	tmp_file_header _fileheader{ 0 };
	std::vector<tmp_image_header> _imageheaders;
	std::vector<uint32_t> _original_offsets;
};

struct rectangle
{
	int32_t x;
	int32_t y;
	size_t width;
	size_t height;
};

struct shp_file_header
{
	uint16_t type;
	uint16_t width;
	uint16_t height;
	uint16_t frames;
};

struct shp_frame_header
{
	int16_t x;
	int16_t y;
	uint16_t width;
	uint16_t height;
	uint32_t flags;
	uint32_t color;
	uint32_t _reserved;
	uint32_t data_offset;
};

class shpfile
{
public:
	shpfile() = default;
	shpfile(std::string filename);
	~shpfile() = default;

	//load and clear
	void clear();
	bool is_loaded();
	bool load(std::string filename);

	//data accessing
	size_t frame_count();
	byte* pixel_data(size_t index);
	rectangle frame_bound(size_t index);
	rectangle file_bound();

	//data modifier
	bool color_replace(std::vector<byte> replace_scheme);

	//save
	size_t calculate_file_size();
	bool save(std::string filename);

private:
	shp_file_header _fileheader{ 0 };
	std::vector<shp_frame_header> _frameheaders;
	std::vector<byte> _pixels;
};

class palette
{
public:
	palette() = default;
	palette(std::string filename);
	~palette() = default;
	
	bool load(std::string filename);
	void clear();
	bool is_loaded();
	std::vector<byte> convert_color(palette& target);
	color& operator[](size_t index);

private:
	std::vector<color> _entries;
};

CLASSES_END