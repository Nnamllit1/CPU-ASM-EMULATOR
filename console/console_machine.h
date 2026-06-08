#pragma once

#include "hardware_profile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>

namespace console {

inline constexpr uint16_t RAM_BANK_REGISTER = 0xFF00;
inline constexpr uint16_t VRAM_BANK_REGISTER = 0xFF01;
inline constexpr uint16_t ROM_BANK_REGISTER = 0xFF02;
inline constexpr uint16_t PPU_CONTROL_REGISTER = 0xFF10;
inline constexpr uint16_t PPU_STATUS_REGISTER = 0xFF11;
inline constexpr uint16_t PPU_PRESENT_REGISTER = 0xFF12;
inline constexpr uint16_t PPU_SPRITE_COUNT_REGISTER = 0xFF13;
inline constexpr uint16_t PPU_SPRITE_COUNT_HIGH_REGISTER = 0xFF14;
inline constexpr uint16_t INPUT_STATUS_REGISTER = 0xFF20;
inline constexpr uint16_t INPUT_DATA_REGISTER = 0xFF21;
inline constexpr uint16_t PROFILE_ID_REGISTER = 0xFF30;
inline constexpr uint16_t FAULT_CODE_REGISTER = 0xFF31;

enum class MachineState {
	Ready,
	Running,
	Paused,
	Halted,
	Faulted,
};

enum class FaultCode : uint8_t {
	None = 0,
	InvalidRegister = 1,
	DivisionByZero = 2,
	UnknownOpcode = 3,
	RomTooLarge = 4,
	InvalidProfile = 5,
	ProgramCounterOutsideRom = 6,
	PpuLimitExceeded = 7,
};

class ConsoleMachine {
public:
	explicit ConsoleMachine(HardwareProfile profile = pocketProfile());

	bool configure(const HardwareProfile& profile, std::string& error);
	bool loadRom(const std::vector<uint8_t>& bytes, uint16_t entryPoint, std::string& error);
	void reset();
	void run();
	void pause();
	bool step();
	size_t runForCycles(uint64_t cycleBudget);

	void addBreakpoint(uint16_t address);
	void removeBreakpoint(uint16_t address);
	void clearBreakpoints();
	bool hasBreakpoint(uint16_t address) const;
	const std::set<uint16_t>& breakpoints() const { return breakpoints_; }
	uint64_t peekInstruction(uint16_t address) const { return fetchInstruction(address); }

	void queueInput(uint16_t value);
	uint8_t readByte(uint16_t address) const;
	void writeByte(uint16_t address, uint8_t value);
	uint16_t readWord(uint16_t address) const;
	void writeWord(uint16_t address, uint16_t value);

	const HardwareProfile& profile() const { return profile_; }
	MachineState state() const { return state_; }
	FaultCode faultCode() const { return faultCode_; }
	const std::string& faultMessage() const { return faultMessage_; }
	const std::array<uint16_t, 32>& registers() const { return registers_; }
	std::array<uint16_t, 32>& registers() { return registers_; }
	uint16_t pc() const { return pc_; }
	void setPc(uint16_t value) { pc_ = value; }
	uint16_t sp() const { return sp_; }
	void setSp(uint16_t value) { sp_ = value; }
	uint64_t cycles() const { return cycles_; }
	uint64_t frames() const { return frames_; }
	uint8_t ramBank() const { return ramBank_; }
	uint8_t vramBank() const { return vramBank_; }
	uint8_t romBank() const { return romBank_; }
	const std::vector<uint8_t>& ram() const { return ram_; }
	const std::vector<uint8_t>& vram() const { return vram_; }
	const std::vector<uint8_t>& rom() const { return rom_; }
	const std::vector<uint32_t>& framebuffer() const { return framebuffer_; }
	const std::string& output() const { return output_; }
	void clearOutput() { output_.clear(); }

private:
	uint8_t fetchRomByte(uint16_t address) const;
	uint64_t fetchInstruction(uint16_t address) const;
	uint32_t executeInstruction(uint64_t instruction);
	void refreshFramebuffer();
	void advanceDevices(uint32_t consumedCycles);
	void fault(FaultCode code, std::string message);
	size_t mappedRamIndex(uint16_t address) const;
	size_t mappedVramIndex(uint16_t address) const;

	HardwareProfile profile_;
	MachineState state_ = MachineState::Ready;
	FaultCode faultCode_ = FaultCode::None;
	std::string faultMessage_;
	std::array<uint16_t, 32> registers_{};
	uint16_t pc_ = 0;
	uint16_t entryPoint_ = 0;
	uint16_t sp_ = 0xFFFE;
	uint64_t cycles_ = 0;
	uint64_t cyclesIntoFrame_ = 0;
	uint64_t frames_ = 0;
	uint8_t ramBank_ = 0;
	uint8_t vramBank_ = 0;
	uint8_t romBank_ = 0;
	uint8_t ppuControl_ = 1;
	uint16_t spriteCount_ = 0;
	std::vector<uint8_t> ram_;
	std::vector<uint8_t> vram_;
	std::vector<uint8_t> rom_;
	std::vector<uint32_t> framebuffer_;
	std::deque<uint16_t> inputQueue_;
	std::set<uint16_t> breakpoints_;
	std::string output_;
};

} // namespace console
