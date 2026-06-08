#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace console {

struct ImportedImage {
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t> rgb332;
};

bool importPpmRgb332(const std::string& path, ImportedImage& image, std::string& error);
bool writeRgb332(const std::string& path, const ImportedImage& image, std::string& error);

} // namespace console
