#include "Classes.h"

#include <time.h>

int main(int argc, const char** argv)
{
	if (argc < 2)
		return 1;
	
	thomas::palette srcpal("source.pal");
	thomas::palette tarpal("target.pal");
	if (!srcpal.is_loaded() || !tarpal.is_loaded())
	{
		std::cout << "Palettes not loaded.\n";
		return 1;
	}

	auto replace_scheme = srcpal.convert_color(tarpal);
	if (replace_scheme.empty())
	{
		std::cout << "Failed to replace colors between palettes.\n";
		return 1;
	}
	
	uint32_t starttime = timeGetTime();
	for (int i = 1; i < argc; i++)
	{
		thomas::tmpfile file(argv[i]);
		if (!file.is_loaded())
			std::cout << "File : " << argv[i] << " is not loaded.\n";

		file.color_replace(replace_scheme);
		file.save(argv[i]);
	}

	std::cout << "All conversions for loaded files complete.\n";
	std::cout << "Time elapsed : " << (timeGetTime() - starttime) / 1000.0 << " s.\n";

	system("pause");
	return 0;
}