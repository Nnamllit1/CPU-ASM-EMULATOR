#include "hardware_profile.h"

#include <limits>

namespace console {

HardwareProfile pocketProfile() {
	return {
		ProfileId::Pocket,
		"Pocket Color",
		4'000'000,
		160,
		144,
		256 * 1024,
		1024 * 1024,
		64 * 1024,
		64 * 1024,
		40,
		10,
		32,
		4,
		60,
	};
}

HardwareProfile homeProfile() {
	return {
		ProfileId::Home,
		"Home 16",
		8'000'000,
		320,
		240,
		1024 * 1024,
		4 * 1024 * 1024,
		256 * 1024,
		1024 * 1024,
		128,
		32,
		256,
		8,
		60,
	};
}

HardwareProfile studioProfile() {
	return {
		ProfileId::Studio,
		"Studio",
		100'000'000,
		640,
		360,
		16 * 1024 * 1024,
		32 * 1024 * 1024,
		16 * 1024 * 1024,
		32 * 1024 * 1024,
		4096,
		1024,
		256,
		32,
		60,
	};
}

HardwareProfile profileFor(ProfileId id) {
	switch (id) {
	case ProfileId::Pocket: return pocketProfile();
	case ProfileId::Home: return homeProfile();
	case ProfileId::Studio: return studioProfile();
	}
	return pocketProfile();
}

bool validateProfile(const HardwareProfile& profile, std::string& error) {
	if (profile.clockHz < 1 || profile.clockHz > 1'000'000'000ULL) {
		error = "CPU frequency must be between 1 Hz and 1 GHz.";
		return false;
	}
	if (profile.displayWidth < 64 || profile.displayWidth > 3840 ||
		profile.displayHeight < 64 || profile.displayHeight > 2160) {
		error = "Display dimensions must be between 64x64 and 3840x2160.";
		return false;
	}
	if (profile.ramBytes < 64 * 1024 || profile.ramBytes > 256ULL * 1024 * 1024) {
		error = "RAM must be between 64 KiB and 256 MiB.";
		return false;
	}
	if (profile.romBytes < 64 * 1024 || profile.romBytes > 512ULL * 1024 * 1024) {
		error = "ROM must be between 64 KiB and 512 MiB.";
		return false;
	}
	const size_t framebufferBytes = static_cast<size_t>(profile.displayWidth) * profile.displayHeight;
	if (profile.vramBytes < framebufferBytes || profile.vramBytes > 256ULL * 1024 * 1024) {
		error = "VRAM must fit one RGB332 framebuffer and cannot exceed 256 MiB.";
		return false;
	}
	if (profile.storageBytes > 256ULL * 1024 * 1024) {
		error = "Persistent storage cannot exceed 256 MiB.";
		return false;
	}
	if (profile.framesPerSecond < 1 || profile.framesPerSecond > 240) {
		error = "Frame rate must be between 1 and 240 Hz.";
		return false;
	}
	if (profile.spritesPerScanline > profile.maxSprites) {
		error = "Per-scanline sprite limit cannot exceed the total sprite limit.";
		return false;
	}
	if (profile.paletteColors < 2 || profile.paletteColors > 256) {
		error = "Palette size must be between 2 and 256 colors.";
		return false;
	}
	if (profile.audioChannels > 64) {
		error = "Audio channel count cannot exceed 64.";
		return false;
	}
	error.clear();
	return true;
}

} // namespace console
