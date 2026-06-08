#include "console/console_machine.h"
#include "console/project.h"
#include "console/source_builder.h"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

uint64_t encode(uint16_t opcode, uint8_t rx = 0, uint8_t ry = 0, uint8_t rz = 0, uint16_t special = 0) {
	return (static_cast<uint64_t>(opcode) << 48) |
		(static_cast<uint64_t>(rx) << 40) |
		(static_cast<uint64_t>(ry) << 32) |
		(static_cast<uint64_t>(rz) << 24) |
		special;
}

void appendInstruction(std::vector<uint8_t>& rom, uint64_t instruction) {
	for (int byte = 7; byte >= 0; --byte) {
		rom.push_back(static_cast<uint8_t>((instruction >> (byte * 8)) & 0xFF));
	}
}

bool expect(bool condition, const std::string& message) {
	if (!condition) std::cerr << "FAIL: " << message << '\n';
	return condition;
}

} // namespace

int main() {
	bool ok = true;
	console::ConsoleMachine machine(console::pocketProfile());
	std::vector<uint8_t> rom;
	appendInstruction(rom, encode(0x0000, 0, 0, 0, 0xE0));
	appendInstruction(rom, encode(0x0000, 1, 0, 0, 0xC000));
	appendInstruction(rom, encode(0x0024, 1, 0));
	appendInstruction(rom, encode(0x0011, 0, 0, 0, console::PPU_PRESENT_REGISTER));
	appendInstruction(rom, encode(0x000C));

	std::string error;
	ok &= expect(machine.loadRom(rom, 0, error), "ROM should load");
	machine.run();
	ok &= expect(machine.runForCycles(100) == 5, "five instructions should execute");
	ok &= expect(machine.state() == console::MachineState::Halted, "machine should halt");
	ok &= expect(machine.cycles() == 9, "deterministic cycle cost should total nine");
	ok &= expect(machine.vram()[0] == 0xE0, "VRAM byte should be written");
	ok &= expect(machine.framebuffer()[0] == 0xDA0000FF, "Pocket palette limit should quantize RGB332 red");

	machine.reset();
	machine.writeByte(console::RAM_BANK_REGISTER, 1);
	machine.writeByte(0x8000, 0x42);
	machine.writeByte(console::RAM_BANK_REGISTER, 2);
	ok &= expect(machine.readByte(0x8000) == 0, "RAM banks should be isolated");
	machine.writeByte(console::RAM_BANK_REGISTER, 1);
	ok &= expect(machine.readByte(0x8000) == 0x42, "RAM bank data should persist");

	machine.addBreakpoint(0);
	machine.run();
	ok &= expect(machine.runForCycles(100) == 0, "breakpoint should stop before execution");
	ok &= expect(machine.state() == console::MachineState::Paused, "breakpoint should pause machine");

	machine.reset();
	machine.writeByte(console::PPU_SPRITE_COUNT_REGISTER, 41);
	machine.writeByte(console::PPU_PRESENT_REGISTER, 1);
	ok &= expect(machine.state() == console::MachineState::Faulted &&
		machine.faultCode() == console::FaultCode::PpuLimitExceeded,
		"Pocket profile should fault above forty sprites");

	auto studio = console::studioProfile();
	studio.clockHz = 0;
	ok &= expect(!console::validateProfile(studio, error), "zero-Hz profile should be rejected");
	studio = console::studioProfile();
	studio.clockHz = 1'000'000'000ULL;
	ok &= expect(console::validateProfile(studio, error), "1 GHz profile should be accepted");

	console::ConsoleProject project;
	project.name = "Round trip";
	project.profile = console::ProfileId::Studio;
	project.studio.clockHz = 1;
	const auto path = std::filesystem::temp_directory_path() / "cpu-asm-console-test.json";
	ok &= expect(console::saveProject(path.string(), project, error), "project should save");
	console::ConsoleProject loaded;
	ok &= expect(console::loadProject(path.string(), loaded, error), "project should load");
	ok &= expect(loaded.profile == console::ProfileId::Studio && loaded.studio.clockHz == 1,
		"project profile should round-trip");
	std::filesystem::remove(path);

	const auto build = console::buildAssemblySource("start:\n    movi r0, 72\n    out r0\n    hlt\n");
	ok &= expect(build.success && !build.rom.empty(), "assembly source should build for the console core");
	console::ConsoleMachine assembledMachine(console::pocketProfile());
	ok &= expect(assembledMachine.loadRom(build.rom, build.entryPoint, error), "assembled ROM should load");
	assembledMachine.run();
	assembledMachine.runForCycles(100);
	ok &= expect(assembledMachine.output().find('H') != std::string::npos, "assembled program should execute");

	if (ok) std::cout << "All console core tests passed.\n";
	return ok ? 0 : 1;
}
