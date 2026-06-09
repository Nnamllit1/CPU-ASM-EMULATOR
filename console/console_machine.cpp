#include "console_machine.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace console {
namespace {

constexpr uint16_t FixedRamEnd = 0x7FFF;
constexpr uint16_t BankedRamStart = 0x8000;
constexpr uint16_t BankedRamEnd = 0xBFFF;
constexpr uint16_t VramStart = 0xC000;
constexpr uint16_t VramEnd = 0xDFFF;
constexpr uint16_t StorageStart = 0xE000;
constexpr uint16_t StorageEnd = 0xFEFF;
constexpr uint16_t DeviceStart = 0xFF00;
constexpr size_t RamBankWindow = 0x4000;
constexpr size_t VramBankWindow = 0x2000;
constexpr size_t StorageBankWindow = 0x1F00;
constexpr size_t RomBankWindow = 0x8000;
constexpr size_t TileMapOffset = 0x4000;
constexpr size_t SpriteTableOffset = 0x5000;
constexpr size_t SpriteDescriptorBytes = 6;

uint32_t rgb332ToRgba(uint8_t color) {
	const uint8_t red = static_cast<uint8_t>(((color >> 5) & 0x07) * 255 / 7);
	const uint8_t green = static_cast<uint8_t>(((color >> 2) & 0x07) * 255 / 7);
	const uint8_t blue = static_cast<uint8_t>((color & 0x03) * 255 / 3);
	return (static_cast<uint32_t>(red) << 24) |
		(static_cast<uint32_t>(green) << 16) |
		(static_cast<uint32_t>(blue) << 8) | 0xFF;
}

} // namespace

ConsoleMachine::ConsoleMachine(HardwareProfile profile) {
	std::string error;
	if (!configure(profile, error)) {
		configure(pocketProfile(), error);
	}
}

bool ConsoleMachine::configure(const HardwareProfile& profile, std::string& error) {
	if (!validateProfile(profile, error)) {
		return false;
	}

	profile_ = profile;
	ram_.assign(profile_.ramBytes, 0);
	vram_.assign(profile_.vramBytes, 0);
	rom_.assign(profile_.romBytes, 0);
	storage_.assign(profile_.storageBytes, 0);
	framebuffer_.assign(static_cast<size_t>(profile_.displayWidth) * profile_.displayHeight, 0x000000FF);
	audioChannels_.assign(profile_.audioChannels, AudioChannel{});
	entryPoint_ = 0;
	reset();
	return true;
}

bool ConsoleMachine::loadRom(const std::vector<uint8_t>& bytes, uint16_t entryPoint, std::string& error) {
	if (bytes.size() > rom_.size()) {
		error = "ROM image exceeds the selected hardware profile limit.";
		fault(FaultCode::RomTooLarge, error);
		return false;
	}
	std::fill(rom_.begin(), rom_.end(), 0);
	std::copy(bytes.begin(), bytes.end(), rom_.begin());
	entryPoint_ = entryPoint;
	reset();
	error.clear();
	return true;
}

void ConsoleMachine::reset() {
	registers_.fill(0);
	std::fill(ram_.begin(), ram_.end(), 0);
	std::fill(vram_.begin(), vram_.end(), 0);
	pc_ = entryPoint_;
	sp_ = 0xFFFE;
	cycles_ = 0;
	scanlinePhase_ = 0;
	frames_ = 0;
	currentScanline_ = 0;
	ramBank_ = 0;
	vramBank_ = 0;
	romBank_ = 0;
	storageBank_ = 0;
	ppuControl_ = 1;
	spriteCount_ = 0;
	selectedAudioChannel_ = 0;
	std::fill(audioChannels_.begin(), audioChannels_.end(), AudioChannel{});
	audioCycleAccumulator_ = 0;
	audioSamples_.clear();
	state_ = MachineState::Ready;
	faultCode_ = FaultCode::None;
	faultMessage_.clear();
	inputQueue_.clear();
	inputButtons_ = 0;
	skipBreakpointOnce_ = false;
	output_.clear();
	refreshFramebuffer();
}

double ConsoleMachine::scanlineProgress() const {
	return profile_.clockHz == 0 ? 0.0 : static_cast<double>(scanlinePhase_) / profile_.clockHz;
}

void ConsoleMachine::run() {
	if (state_ != MachineState::Faulted && state_ != MachineState::Halted) {
		skipBreakpointOnce_ = state_ == MachineState::Paused && hasBreakpoint(pc_);
		state_ = MachineState::Running;
	}
}

void ConsoleMachine::pause() {
	if (state_ == MachineState::Running) {
		state_ = MachineState::Paused;
	}
}

bool ConsoleMachine::step() {
	if (state_ == MachineState::Faulted || state_ == MachineState::Halted) {
		return false;
	}
	if (hasBreakpoint(pc_) && state_ == MachineState::Running && !skipBreakpointOnce_) {
		state_ = MachineState::Paused;
		return false;
	}
	skipBreakpointOnce_ = false;

	const uint32_t consumedCycles = executeInstruction(fetchInstruction(pc_));
	if (consumedCycles == 0) {
		return false;
	}
	cycles_ += consumedCycles;
	advanceDevices(consumedCycles);
	return true;
}

size_t ConsoleMachine::runForCycles(uint64_t cycleBudget) {
	if (state_ != MachineState::Running) {
		return 0;
	}
	const uint64_t target = cycles_ + cycleBudget;
	size_t executed = 0;
	while (state_ == MachineState::Running && cycles_ < target) {
		if (!step()) {
			break;
		}
		++executed;
	}
	return executed;
}

void ConsoleMachine::addBreakpoint(uint16_t address) { breakpoints_.insert(address); }
void ConsoleMachine::removeBreakpoint(uint16_t address) { breakpoints_.erase(address); }
void ConsoleMachine::clearBreakpoints() { breakpoints_.clear(); }
bool ConsoleMachine::hasBreakpoint(uint16_t address) const { return breakpoints_.contains(address); }
void ConsoleMachine::queueInput(uint16_t value) { inputQueue_.push_back(value); }

bool ConsoleMachine::loadStorageFile(const std::string& path, std::string& error) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		error = "Could not open storage file: " + path;
		return false;
	}
	std::fill(storage_.begin(), storage_.end(), 0);
	input.read(reinterpret_cast<char*>(storage_.data()), static_cast<std::streamsize>(storage_.size()));
	error.clear();
	return true;
}

bool ConsoleMachine::saveStorageFile(const std::string& path, std::string& error) const {
	std::ofstream output(path, std::ios::binary);
	if (!output) {
		error = "Could not write storage file: " + path;
		return false;
	}
	output.write(reinterpret_cast<const char*>(storage_.data()), static_cast<std::streamsize>(storage_.size()));
	if (!output.good()) {
		error = "Failed while writing storage file: " + path;
		return false;
	}
	error.clear();
	return true;
}

bool ConsoleMachine::loadVram(const std::vector<uint8_t>& bytes, size_t offset, std::string& error) {
	if (offset > vram_.size() || bytes.size() > vram_.size() - offset) {
		error = "Asset does not fit in profile VRAM.";
		return false;
	}
	std::copy(bytes.begin(), bytes.end(), vram_.begin() + static_cast<std::ptrdiff_t>(offset));
	restartScanout();
	error.clear();
	return true;
}

std::vector<float> ConsoleMachine::drainAudioSamples() {
	std::vector<float> samples;
	samples.swap(audioSamples_);
	return samples;
}

size_t ConsoleMachine::mappedRamIndex(uint16_t address) const {
	if (address <= FixedRamEnd) {
		return static_cast<size_t>(address) % ram_.size();
	}
	if (address >= BankedRamStart && address <= BankedRamEnd) {
		const size_t bankStorageStart = 0x8000;
		const size_t bankCount = std::max<size_t>(1, (ram_.size() - bankStorageStart) / RamBankWindow);
		return bankStorageStart + (static_cast<size_t>(ramBank_) % bankCount) * RamBankWindow + (address - BankedRamStart);
	}
	return static_cast<size_t>(address) % ram_.size();
}

size_t ConsoleMachine::mappedVramIndex(uint16_t address) const {
	const size_t bankCount = std::max<size_t>(1, vram_.size() / VramBankWindow);
	return ((static_cast<size_t>(vramBank_) % bankCount) * VramBankWindow + (address - VramStart)) % vram_.size();
}

size_t ConsoleMachine::mappedStorageIndex(uint16_t address) const {
	if (storage_.empty()) return 0;
	const size_t bankCount = std::max<size_t>(1, storage_.size() / StorageBankWindow);
	return ((static_cast<size_t>(storageBank_) % bankCount) * StorageBankWindow + (address - StorageStart)) % storage_.size();
}

uint8_t ConsoleMachine::readByte(uint16_t address) const {
	const uint64_t wholeSeconds = cycles_ / profile_.clockHz;
	const uint64_t remainingCycles = cycles_ % profile_.clockHz;
	const uint16_t milliseconds = static_cast<uint16_t>((wholeSeconds * 1000 + remainingCycles * 1000 / profile_.clockHz) & 0xFFFF);
	const uint16_t frameCounter = static_cast<uint16_t>(frames_ & 0xFFFF);
	const uint16_t cycleCounter = static_cast<uint16_t>(cycles_ & 0xFFFF);
	if (address >= VramStart && address <= VramEnd) {
		return vram_[mappedVramIndex(address)];
	}
	if (address >= StorageStart && address <= StorageEnd) {
		return storage_.empty() ? 0 : storage_[mappedStorageIndex(address)];
	}
	if (address >= DeviceStart) {
		switch (address) {
		case RAM_BANK_REGISTER: return ramBank_;
		case VRAM_BANK_REGISTER: return vramBank_;
		case ROM_BANK_REGISTER: return romBank_;
		case STORAGE_BANK_REGISTER: return storageBank_;
		case PPU_CONTROL_REGISTER: return ppuControl_;
		case PPU_STATUS_REGISTER: return currentScanline_ == 0 ? 1 : 0;
		case PPU_SPRITE_COUNT_REGISTER: return static_cast<uint8_t>(spriteCount_ & 0xFF);
		case PPU_SPRITE_COUNT_HIGH_REGISTER: return static_cast<uint8_t>((spriteCount_ >> 8) & 0xFF);
		case INPUT_STATUS_REGISTER: return inputQueue_.empty() ? 0 : 1;
		case INPUT_DATA_REGISTER: return inputQueue_.empty() ? 0 : static_cast<uint8_t>(inputQueue_.front() & 0xFF);
		case INPUT_BUTTONS_LOW_REGISTER: return static_cast<uint8_t>(inputButtons_ & 0xFF);
		case INPUT_BUTTONS_HIGH_REGISTER: return static_cast<uint8_t>((inputButtons_ >> 8) & 0xFF);
		case PROFILE_ID_REGISTER: return static_cast<uint8_t>(profile_.id);
		case FAULT_CODE_REGISTER: return static_cast<uint8_t>(faultCode_);
		case AUDIO_CHANNEL_REGISTER: return selectedAudioChannel_;
		case AUDIO_CONTROL_REGISTER:
			return audioChannels_.empty() ? 0 : static_cast<uint8_t>(audioChannels_[selectedAudioChannel_ % audioChannels_.size()].enabled);
		case AUDIO_FREQUENCY_LOW_REGISTER:
			return audioChannels_.empty() ? 0 : static_cast<uint8_t>(audioChannels_[selectedAudioChannel_ % audioChannels_.size()].frequency & 0xFF);
		case AUDIO_FREQUENCY_HIGH_REGISTER:
			return audioChannels_.empty() ? 0 : static_cast<uint8_t>((audioChannels_[selectedAudioChannel_ % audioChannels_.size()].frequency >> 8) & 0xFF);
		case AUDIO_VOLUME_REGISTER:
			return audioChannels_.empty() ? 0 : audioChannels_[selectedAudioChannel_ % audioChannels_.size()].volume;
		case TIMER_MILLISECONDS_HIGH_REGISTER: return static_cast<uint8_t>((milliseconds >> 8) & 0xFF);
		case TIMER_MILLISECONDS_LOW_REGISTER: return static_cast<uint8_t>(milliseconds & 0xFF);
		case TIMER_FRAMES_HIGH_REGISTER: return static_cast<uint8_t>((frameCounter >> 8) & 0xFF);
		case TIMER_FRAMES_LOW_REGISTER: return static_cast<uint8_t>(frameCounter & 0xFF);
		case TIMER_CYCLES_HIGH_REGISTER: return static_cast<uint8_t>((cycleCounter >> 8) & 0xFF);
		case TIMER_CYCLES_LOW_REGISTER: return static_cast<uint8_t>(cycleCounter & 0xFF);
		default: return 0;
		}
	}
	return ram_[mappedRamIndex(address)];
}

void ConsoleMachine::writeByte(uint16_t address, uint8_t value) {
	if (address >= VramStart && address <= VramEnd) {
		vram_[mappedVramIndex(address)] = value;
		return;
	}
	if (address >= StorageStart && address <= StorageEnd) {
		if (!storage_.empty()) storage_[mappedStorageIndex(address)] = value;
		return;
	}
	if (address >= DeviceStart) {
		switch (address) {
		case RAM_BANK_REGISTER: ramBank_ = value; break;
		case VRAM_BANK_REGISTER: vramBank_ = value; break;
		case ROM_BANK_REGISTER: romBank_ = value; break;
		case STORAGE_BANK_REGISTER: storageBank_ = value; break;
		case PPU_CONTROL_REGISTER: ppuControl_ = value; break;
		case PPU_PRESENT_REGISTER: restartScanout(); break;
		case PPU_SPRITE_COUNT_REGISTER: spriteCount_ = static_cast<uint16_t>((spriteCount_ & 0xFF00) | value); break;
		case PPU_SPRITE_COUNT_HIGH_REGISTER: spriteCount_ = static_cast<uint16_t>((spriteCount_ & 0x00FF) | (value << 8)); break;
		case INPUT_DATA_REGISTER:
			if (!inputQueue_.empty()) inputQueue_.pop_front();
			break;
		case AUDIO_CHANNEL_REGISTER:
			selectedAudioChannel_ = audioChannels_.empty() ? 0 : static_cast<uint8_t>(value % audioChannels_.size());
			break;
		case AUDIO_CONTROL_REGISTER:
			if (!audioChannels_.empty()) audioChannels_[selectedAudioChannel_].enabled = (value & 1) != 0;
			break;
		case AUDIO_FREQUENCY_LOW_REGISTER:
			if (!audioChannels_.empty()) audioChannels_[selectedAudioChannel_].frequency =
				static_cast<uint16_t>((audioChannels_[selectedAudioChannel_].frequency & 0xFF00) | value);
			break;
		case AUDIO_FREQUENCY_HIGH_REGISTER:
			if (!audioChannels_.empty()) audioChannels_[selectedAudioChannel_].frequency =
				static_cast<uint16_t>((audioChannels_[selectedAudioChannel_].frequency & 0x00FF) | (value << 8));
			break;
		case AUDIO_VOLUME_REGISTER:
			if (!audioChannels_.empty()) audioChannels_[selectedAudioChannel_].volume = value;
			break;
		default: break;
		}
		return;
	}
	ram_[mappedRamIndex(address)] = value;
}

uint16_t ConsoleMachine::readWord(uint16_t address) const {
	return static_cast<uint16_t>((static_cast<uint16_t>(readByte(address)) << 8) |
		readByte(static_cast<uint16_t>(address + 1)));
}

void ConsoleMachine::writeWord(uint16_t address, uint16_t value) {
	writeByte(address, static_cast<uint8_t>((value >> 8) & 0xFF));
	writeByte(static_cast<uint16_t>(address + 1), static_cast<uint8_t>(value & 0xFF));
}

uint8_t ConsoleMachine::fetchRomByte(uint16_t address) const {
	size_t index = address;
	if (address >= 0x8000) {
		const size_t bankCount = std::max<size_t>(1, rom_.size() / RomBankWindow);
		const size_t switchableBanks = std::max<size_t>(1, bankCount - 1);
		const size_t bank = 1 + (static_cast<size_t>(romBank_) % switchableBanks);
		index = bank * RomBankWindow + (address - 0x8000);
	}
	if (index >= rom_.size()) {
		return 0;
	}
	return rom_[index];
}

uint64_t ConsoleMachine::fetchInstruction(uint16_t address) const {
	uint64_t instruction = 0;
	for (int byte = 0; byte < 8; ++byte) {
		instruction = (instruction << 8) | fetchRomByte(static_cast<uint16_t>(address + byte));
	}
	return instruction;
}

uint32_t ConsoleMachine::executeInstruction(uint64_t instruction) {
	const uint16_t opcode = static_cast<uint16_t>((instruction >> 48) & 0xFFFF);
	const uint8_t rx = static_cast<uint8_t>((instruction >> 40) & 0xFF);
	const uint8_t ry = static_cast<uint8_t>((instruction >> 32) & 0xFF);
	const uint8_t rz = static_cast<uint8_t>((instruction >> 24) & 0xFF);
	const uint16_t special = static_cast<uint16_t>(instruction & 0xFFFF);
	if (rx >= registers_.size() || ry >= registers_.size() || rz >= registers_.size()) {
		fault(FaultCode::InvalidRegister, "Instruction referenced an invalid register.");
		return 0;
	}

	auto advance = [this]() { pc_ = static_cast<uint16_t>(pc_ + 8); };
	auto branch = [this](uint16_t address) { pc_ = address; };
	auto memoryCost = [](uint16_t address) { return address >= DeviceStart ? 4U : 2U; };
	uint32_t cost = 1;

	switch (opcode) {
	case 0x0000: registers_[rx] = special; advance(); break;
	case 0x0001: registers_[rx] = registers_[ry]; advance(); break;
	case 0x0002: registers_[rx] = registers_[ry]; registers_[ry] = 0; advance(); break;
	case 0x0003: registers_[rx] = static_cast<uint16_t>(registers_[ry] + registers_[rz]); advance(); break;
	case 0x0004: registers_[rx] = static_cast<uint16_t>(registers_[ry] - registers_[rz]); advance(); break;
	case 0x0005: registers_[rx] = registers_[rz] >= 16 ? 0 : static_cast<uint16_t>(registers_[ry] << registers_[rz]); advance(); break;
	case 0x0006: registers_[rx] = registers_[rz] >= 16 ? 0 : static_cast<uint16_t>(registers_[ry] >> registers_[rz]); advance(); break;
	case 0x0007: branch(special); cost = 2; break;
	case 0x0008: if (registers_[rx] == 0) { branch(special); cost = 2; } else advance(); break;
	case 0x0009: if (registers_[rx] != 0) { branch(special); cost = 2; } else advance(); break;
	case 0x000A: if (registers_[rx] == registers_[ry]) { branch(special); cost = 2; } else advance(); break;
	case 0x000B: if (registers_[rx] != registers_[ry]) { branch(special); cost = 2; } else advance(); break;
	case 0x000C: state_ = MachineState::Halted; output_ += "HLT encountered. Stopping execution.\n"; cost = 1; break;
	case 0x000D: if (registers_[rx] <= 127) output_.push_back(static_cast<char>(registers_[rx])); advance(); cost = 4; break;
	case 0x000E: cost = memoryCost(registers_[ry]); registers_[rx] = readWord(registers_[ry]); advance(); break;
	case 0x000F: cost = memoryCost(special); registers_[rx] = readWord(special); advance(); break;
	case 0x0010: cost = memoryCost(registers_[rx]); writeWord(registers_[rx], registers_[ry]); advance(); break;
	case 0x0011: cost = memoryCost(special); writeWord(special, registers_[rx]); advance(); break;
	case 0x0012: registers_[rx] = static_cast<uint16_t>(registers_[ry] * registers_[rz]); advance(); break;
	case 0x0013:
		if (registers_[rz] == 0) { fault(FaultCode::DivisionByZero, "Division by zero."); return 0; }
		registers_[rx] = static_cast<uint16_t>(registers_[ry] / registers_[rz]); advance(); break;
	case 0x0014:
		if (registers_[rz] == 0) { fault(FaultCode::DivisionByZero, "Modulo by zero."); return 0; }
		registers_[rx] = static_cast<uint16_t>(registers_[ry] % registers_[rz]); advance(); break;
	case 0x0015: registers_[rx] = static_cast<uint16_t>(registers_[ry] & registers_[rz]); advance(); break;
	case 0x0016: registers_[rx] = static_cast<uint16_t>(registers_[ry] | registers_[rz]); advance(); break;
	case 0x0017: registers_[rx] = static_cast<uint16_t>(registers_[ry] ^ registers_[rz]); advance(); break;
	case 0x0018: registers_[rx] = static_cast<uint16_t>(~registers_[ry]); advance(); break;
	case 0x0019: if (registers_[rx] < registers_[ry]) { branch(special); cost = 2; } else advance(); break;
	case 0x001A: if (registers_[rx] <= registers_[ry]) { branch(special); cost = 2; } else advance(); break;
	case 0x001B: if (registers_[rx] > registers_[ry]) { branch(special); cost = 2; } else advance(); break;
	case 0x001C: if (registers_[rx] >= registers_[ry]) { branch(special); cost = 2; } else advance(); break;
	case 0x001D: sp_ = static_cast<uint16_t>(sp_ - 2); writeWord(sp_, registers_[rx]); advance(); cost = 3; break;
	case 0x001E: registers_[rx] = readWord(sp_); sp_ = static_cast<uint16_t>(sp_ + 2); advance(); cost = 3; break;
	case 0x001F: sp_ = static_cast<uint16_t>(sp_ - 2); writeWord(sp_, static_cast<uint16_t>(pc_ + 8)); branch(special); cost = 3; break;
	case 0x0020: branch(readWord(sp_)); sp_ = static_cast<uint16_t>(sp_ + 2); cost = 3; break;
	case 0x0021: output_ += std::to_string(registers_[rx]); advance(); cost = 4; break;
	case 0x0022: {
		uint16_t address = registers_[rx];
		while (fetchRomByte(address) != 0) {
			output_.push_back(static_cast<char>(fetchRomByte(address++)));
		}
		advance(); cost = 4; break;
	}
	case 0x0023: cost = memoryCost(registers_[ry]); registers_[rx] = readByte(registers_[ry]); advance(); break;
	case 0x0024: cost = memoryCost(registers_[rx]); writeByte(registers_[rx], static_cast<uint8_t>(registers_[ry])); advance(); break;
	case 0x0025: cost = memoryCost(special); registers_[rx] = readByte(special); advance(); break;
	case 0x0026: cost = memoryCost(special); writeByte(special, static_cast<uint8_t>(registers_[rx])); advance(); break;
	case 0x0027: registers_[rx] = fetchRomByte(registers_[ry]); advance(); cost = 2; break;
	case 0x0028: registers_[rx] = fetchRomByte(special); advance(); cost = 2; break;
	case 0x0029: registers_[rx] = static_cast<uint16_t>((fetchRomByte(registers_[ry]) << 8) | fetchRomByte(static_cast<uint16_t>(registers_[ry] + 1))); advance(); cost = 2; break;
	case 0x002A: registers_[rx] = static_cast<uint16_t>((fetchRomByte(special) << 8) | fetchRomByte(static_cast<uint16_t>(special + 1))); advance(); cost = 2; break;
	case 0x002B:
	case 0x002C:
		if (!inputQueue_.empty()) { registers_[rx] = inputQueue_.front(); inputQueue_.pop_front(); }
		advance(); cost = 4; break;
	default:
		fault(FaultCode::UnknownOpcode, "Unknown opcode: " + std::to_string(opcode));
		return 0;
	}
	return cost;
}

void ConsoleMachine::refreshFramebuffer() {
	for (uint32_t scanline = 0; scanline < profile_.displayHeight; ++scanline) renderScanline(scanline);
}

void ConsoleMachine::renderScanline(uint32_t scanline) {
	if (scanline >= profile_.displayHeight) return;
	const size_t rowStart = static_cast<size_t>(scanline) * profile_.displayWidth;
	const size_t rowEnd = rowStart + profile_.displayWidth;
	if ((ppuControl_ & 1) == 0) {
		std::fill(framebuffer_.begin() + static_cast<std::ptrdiff_t>(rowStart),
			framebuffer_.begin() + static_cast<std::ptrdiff_t>(rowEnd), 0x000000FF);
		return;
	}
	const auto constrainedColor = [this](uint8_t color) {
		return profile_.paletteColors <= 32 ? static_cast<uint8_t>(color & 0xDA) : color;
	};

	if ((ppuControl_ & 0x02) != 0) {
		const uint32_t mapWidth = (profile_.displayWidth + 7) / 8;
		for (uint32_t x = 0; x < profile_.displayWidth; ++x) {
			const size_t mapIndex = TileMapOffset + (scanline / 8) * mapWidth + (x / 8);
			const uint8_t tile = mapIndex < vram_.size() ? vram_[mapIndex] : 0;
			const size_t pixelIndex = static_cast<size_t>(tile) * 64 + (scanline % 8) * 8 + (x % 8);
			const uint8_t color = pixelIndex < vram_.size() ? vram_[pixelIndex] : 0;
			framebuffer_[rowStart + x] = rgb332ToRgba(constrainedColor(color));
		}
	} else {
		for (uint32_t x = 0; x < profile_.displayWidth; ++x) {
			const size_t pixel = rowStart + x;
			const uint8_t color = pixel < vram_.size() ? vram_[pixel] : 0;
			framebuffer_[pixel] = rgb332ToRgba(constrainedColor(color));
		}
	}

	if (spriteCount_ > profile_.maxSprites) {
		fault(FaultCode::PpuLimitExceeded, "Program exceeded the profile sprite limit.");
		return;
	}
	uint32_t spritesOnScanline = 0;
	for (uint32_t sprite = 0; sprite < spriteCount_; ++sprite) {
		const size_t descriptor = SpriteTableOffset + sprite * SpriteDescriptorBytes;
		if (descriptor + SpriteDescriptorBytes > vram_.size()) break;
		const uint16_t x = static_cast<uint16_t>(vram_[descriptor] | (vram_[descriptor + 1] << 8));
		const uint16_t y = static_cast<uint16_t>(vram_[descriptor + 2] | (vram_[descriptor + 3] << 8));
		const uint8_t size = std::clamp<uint8_t>(vram_[descriptor + 5], 1, 32);
		if (scanline < y || scanline >= static_cast<uint32_t>(y) + size) continue;
		if (spritesOnScanline >= profile_.spritesPerScanline) continue;
		++spritesOnScanline;
		const uint8_t color = constrainedColor(vram_[descriptor + 4]);
		for (uint32_t px = x; px < std::min<uint32_t>(profile_.displayWidth, static_cast<uint32_t>(x) + size); ++px) {
			framebuffer_[rowStart + px] = rgb332ToRgba(color);
		}
	}
}

void ConsoleMachine::restartScanout() {
	scanlinePhase_ = 0;
	currentScanline_ = 0;
	renderScanline(0);
}

void ConsoleMachine::advanceScanoutLines(uint64_t lines) {
	if (lines == 0 || profile_.displayHeight == 0) return;
	const uint64_t totalLines = static_cast<uint64_t>(currentScanline_) + lines;
	const uint64_t completedFrames = totalLines / profile_.displayHeight;
	const uint32_t finalScanline = static_cast<uint32_t>(totalLines % profile_.displayHeight);
	if (completedFrames > 0) {
		refreshFramebuffer();
		frames_ += completedFrames;
		currentScanline_ = 0;
	}
	while (currentScanline_ < finalScanline) {
		renderScanline(currentScanline_);
		++currentScanline_;
	}
}

void ConsoleMachine::advanceDevices(uint32_t consumedCycles) {
	generateAudio(consumedCycles);
	const uint64_t scanlineUnits = static_cast<uint64_t>(consumedCycles) *
		profile_.framesPerSecond * profile_.displayHeight;
	const uint64_t accumulated = scanlinePhase_ + scanlineUnits;
	advanceScanoutLines(accumulated / profile_.clockHz);
	scanlinePhase_ = accumulated % profile_.clockHz;
}

void ConsoleMachine::generateAudio(uint32_t consumedCycles) {
	if (audioChannels_.empty() || profile_.clockHz == 0) return;
	constexpr uint64_t sampleRate = 48'000;
	audioCycleAccumulator_ += static_cast<uint64_t>(consumedCycles) * sampleRate;
	while (audioCycleAccumulator_ >= profile_.clockHz && audioSamples_.size() < sampleRate * 2) {
		audioCycleAccumulator_ -= profile_.clockHz;
		double mixed = 0.0;
		size_t activeChannels = 0;
		for (AudioChannel& channel : audioChannels_) {
			if (!channel.enabled || channel.volume == 0 || channel.frequency == 0) continue;
			++activeChannels;
			channel.phase += static_cast<double>(channel.frequency) / sampleRate;
			if (channel.phase >= 1.0) channel.phase -= static_cast<uint64_t>(channel.phase);
			const double wave = channel.phase < 0.5 ? 1.0 : -1.0;
			mixed += wave * (static_cast<double>(channel.volume) / 255.0);
		}
		mixed /= static_cast<double>(std::max<size_t>(1, activeChannels));
		audioSamples_.push_back(static_cast<float>(std::clamp(mixed, -1.0, 1.0)));
	}
}

void ConsoleMachine::fault(FaultCode code, std::string message) {
	faultCode_ = code;
	faultMessage_ = std::move(message);
	state_ = MachineState::Faulted;
}

} // namespace console
