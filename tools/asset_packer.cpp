#include "console/asset_import.h"

#include <iostream>

int main(int argc, char** argv) {
	if (argc != 3) {
		std::cout << "Usage: CPU-ASM-ASSET input.ppm output.rgb332\n";
		return 1;
	}
	console::ImportedImage image;
	std::string error;
	if (!console::importPpmRgb332(argv[1], image, error) || !console::writeRgb332(argv[2], image, error)) {
		std::cerr << error << '\n';
		return 1;
	}
	std::cout << "Packed " << image.width << 'x' << image.height << " image into "
		<< image.rgb332.size() << " RGB332 bytes.\n";
	return 0;
}
