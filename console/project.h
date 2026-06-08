#pragma once

#include "hardware_profile.h"

#include <array>
#include <cstdint>
#include <string>

namespace console {

enum class ConsoleButton : size_t {
	Up,
	Down,
	Left,
	Right,
	A,
	B,
	Start,
	Select,
	Count,
};

inline constexpr size_t ConsoleButtonCount = static_cast<size_t>(ConsoleButton::Count);

struct InputBindings {
	// SDL keycodes and SDL gamepad button identifiers, stored as integers to keep console-core SDL-independent.
	std::array<int32_t, ConsoleButtonCount> keyboard{ 0x40000052, 0x40000051, 0x40000050, 0x4000004F, 'z', 'x', 13, 0x400000E5 };
	std::array<int32_t, ConsoleButtonCount> gamepad{ 11, 12, 13, 14, 0, 1, 6, 4 };
};

struct ConsoleProject {
	std::string name = "Untitled Console Project";
	std::string sourcePath = "main.asm";
	std::string storagePath = "console-storage.sav";
	std::string assetPath;
	ProfileId profile = ProfileId::Pocket;
	HardwareProfile studio = studioProfile();
	InputBindings input;
};

bool loadProject(const std::string& path, ConsoleProject& project, std::string& error);
bool saveProject(const std::string& path, const ConsoleProject& project, std::string& error);

} // namespace console
