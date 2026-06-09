#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace console {

enum class ProfileId : uint8_t {
	Pocket = 0,
	Home = 1,
	Studio = 2,
};

struct HardwareProfile {
	ProfileId id = ProfileId::Pocket;
	std::string name;
	uint64_t clockHz = 4'000'000;
	uint32_t displayWidth = 160;
	uint32_t displayHeight = 144;
	size_t ramBytes = 256 * 1024;
	size_t romBytes = 1024 * 1024;
	size_t vramBytes = 64 * 1024;
	size_t storageBytes = 64 * 1024;
	uint32_t maxSprites = 40;
	uint32_t spritesPerScanline = 10;
	uint32_t paletteColors = 32;
	uint32_t audioChannels = 4;
	uint32_t framesPerSecond = 60;
};

HardwareProfile pocketProfile();
HardwareProfile homeProfile();
HardwareProfile studioProfile();
HardwareProfile profileFor(ProfileId id);
HardwareProfile customProfileFrom(ProfileId reference);
bool validateProfile(const HardwareProfile& profile, std::string& error);

} // namespace console
