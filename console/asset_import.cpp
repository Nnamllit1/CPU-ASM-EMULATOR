#include "asset_import.h"

#include <cctype>
#include <fstream>
#include <limits>

namespace console {
namespace {

bool readToken(std::istream& input, std::string& token) {
	token.clear();
	char c = 0;
	while (input.get(c)) {
		if (c == '#') {
			input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			continue;
		}
		if (!std::isspace(static_cast<unsigned char>(c))) {
			token.push_back(c);
			break;
		}
	}
	while (input.get(c)) {
		if (std::isspace(static_cast<unsigned char>(c))) break;
		token.push_back(c);
	}
	return !token.empty();
}

uint8_t toRgb332(uint32_t red, uint32_t green, uint32_t blue, uint32_t maximum) {
	red = red * 255 / maximum;
	green = green * 255 / maximum;
	blue = blue * 255 / maximum;
	return static_cast<uint8_t>(((red >> 5) << 5) | ((green >> 5) << 2) | (blue >> 6));
}

} // namespace

bool importPpmRgb332(const std::string& path, ImportedImage& image, std::string& error) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		error = "Could not open PPM image: " + path;
		return false;
	}
	std::string token;
	if (!readToken(input, token) || (token != "P3" && token != "P6")) {
		error = "Asset importer supports P3 and P6 PPM images.";
		return false;
	}
	const bool binary = token == "P6";
	if (!readToken(input, token)) return false;
	image.width = static_cast<uint32_t>(std::stoul(token));
	if (!readToken(input, token)) return false;
	image.height = static_cast<uint32_t>(std::stoul(token));
	if (!readToken(input, token)) return false;
	const uint32_t maximum = static_cast<uint32_t>(std::stoul(token));
	if (image.width == 0 || image.height == 0 || maximum == 0 || maximum > 255 ||
		static_cast<uint64_t>(image.width) * image.height > 16ULL * 1024 * 1024) {
		error = "Invalid or excessively large PPM image.";
		return false;
	}

	image.rgb332.clear();
	image.rgb332.reserve(static_cast<size_t>(image.width) * image.height);
	for (uint64_t pixel = 0; pixel < static_cast<uint64_t>(image.width) * image.height; ++pixel) {
		uint32_t red = 0, green = 0, blue = 0;
		if (binary) {
			unsigned char bytes[3]{};
			if (!input.read(reinterpret_cast<char*>(bytes), 3)) {
				error = "PPM pixel data ended early.";
				return false;
			}
			red = bytes[0]; green = bytes[1]; blue = bytes[2];
		} else {
			if (!readToken(input, token)) return false; red = static_cast<uint32_t>(std::stoul(token));
			if (!readToken(input, token)) return false; green = static_cast<uint32_t>(std::stoul(token));
			if (!readToken(input, token)) return false; blue = static_cast<uint32_t>(std::stoul(token));
		}
		image.rgb332.push_back(toRgb332(red, green, blue, maximum));
	}
	error.clear();
	return true;
}

bool writeRgb332(const std::string& path, const ImportedImage& image, std::string& error) {
	std::ofstream output(path, std::ios::binary);
	if (!output) {
		error = "Could not write RGB332 asset: " + path;
		return false;
	}
	output.write(reinterpret_cast<const char*>(image.rgb332.data()), static_cast<std::streamsize>(image.rgb332.size()));
	if (!output.good()) {
		error = "Failed while writing RGB332 asset: " + path;
		return false;
	}
	error.clear();
	return true;
}

} // namespace console
