#pragma once

#include "hardware_profile.h"

#include <string>

namespace console {

struct ConsoleProject {
	std::string name = "Untitled Console Project";
	std::string sourcePath = "main.asm";
	ProfileId profile = ProfileId::Pocket;
	HardwareProfile studio = studioProfile();
};

bool loadProject(const std::string& path, ConsoleProject& project, std::string& error);
bool saveProject(const std::string& path, const ConsoleProject& project, std::string& error);

} // namespace console
