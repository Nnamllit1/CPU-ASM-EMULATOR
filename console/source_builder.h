#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace console {

struct SourceBuildResult {
	bool success = false;
	std::vector<uint8_t> rom;
	uint16_t entryPoint = 0;
	std::string diagnostics;
	std::map<uint16_t, int> addressToSourceLine;
	std::map<int, uint16_t> sourceLineToAddress;
};

SourceBuildResult buildAssemblySource(const std::string& source, bool useDefaultIncludes = true);

} // namespace console
