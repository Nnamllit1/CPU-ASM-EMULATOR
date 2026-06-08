#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace console {

struct SourceBuildResult {
	bool success = false;
	std::vector<uint8_t> rom;
	uint16_t entryPoint = 0;
	std::string diagnostics;
};

SourceBuildResult buildAssemblySource(const std::string& source, bool useDefaultIncludes = true);

} // namespace console
