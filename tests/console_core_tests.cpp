#include "console/console_machine.h"
#include "console/asset_import.h"
#include "console/project.h"
#include "console/source_builder.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
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
	machine.run();
	ok &= expect(machine.runForCycles(1) == 1 && machine.pc() == 8,
		"continuing from a breakpoint should execute the stopped instruction once");

	machine.setInputButtons(0x00A5);
	ok &= expect(machine.readByte(console::INPUT_BUTTONS_LOW_REGISTER) == 0xA5 &&
		machine.readByte(console::INPUT_BUTTONS_HIGH_REGISTER) == 0,
		"held console buttons should be exposed through MMIO");
	machine.reset();
	ok &= expect(machine.inputButtons() == 0, "reset should release held console buttons");

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
	project.input.keyboard[static_cast<size_t>(console::ConsoleButton::A)] = 'k';
	project.input.gamepad[static_cast<size_t>(console::ConsoleButton::Start)] = 9;
	const auto path = std::filesystem::temp_directory_path() / "cpu-asm-console-test.json";
	ok &= expect(console::saveProject(path.string(), project, error), "project should save");
	console::ConsoleProject loaded;
	ok &= expect(console::loadProject(path.string(), loaded, error), "project should load");
	ok &= expect(loaded.profile == console::ProfileId::Studio && loaded.studio.clockHz == 1,
		"project profile should round-trip");
	ok &= expect(loaded.input.keyboard[static_cast<size_t>(console::ConsoleButton::A)] == 'k' &&
		loaded.input.gamepad[static_cast<size_t>(console::ConsoleButton::Start)] == 9,
		"project input bindings should round-trip");
	std::filesystem::remove(path);
	{
		std::ofstream legacy(path);
		legacy << "{\"name\":\"Legacy\",\"source\":\"main.asm\",\"profile\":\"pocket\"}\n";
	}
	console::ConsoleProject legacyProject;
	ok &= expect(console::loadProject(path.string(), legacyProject, error) &&
		legacyProject.input.keyboard[static_cast<size_t>(console::ConsoleButton::A)] == 'z',
		"projects without input bindings should retain default controls");
	std::filesystem::remove(path);

	machine.configure(console::pocketProfile(), error);
	machine.writeByte(console::STORAGE_BANK_REGISTER, 1);
	machine.writeByte(0xE000, 0x5A);
	const auto storagePath = std::filesystem::temp_directory_path() / "cpu-asm-console-storage.sav";
	ok &= expect(machine.saveStorageFile(storagePath.string(), error), "persistent storage should save");
	console::ConsoleMachine storageMachine(console::pocketProfile());
	ok &= expect(storageMachine.loadStorageFile(storagePath.string(), error), "persistent storage should load");
	storageMachine.writeByte(console::STORAGE_BANK_REGISTER, 1);
	ok &= expect(storageMachine.readByte(0xE000) == 0x5A, "storage bank data should round-trip");
	std::filesystem::remove(storagePath);

	const auto ppmPath = std::filesystem::temp_directory_path() / "cpu-asm-console-asset.ppm";
	{
		std::ofstream ppm(ppmPath);
		ppm << "P3\n2 1\n255\n255 0 0 0 0 255\n";
	}
	console::ImportedImage imported;
	ok &= expect(console::importPpmRgb332(ppmPath.string(), imported, error), "PPM asset should import");
	ok &= expect(imported.width == 2 && imported.rgb332.size() == 2 && imported.rgb332[0] == 0xE0,
		"asset should convert to RGB332");
	std::filesystem::remove(ppmPath);

	const auto build = console::buildAssemblySource("start:\n    movi r0, 72\n    out r0\n    hlt\n");
	ok &= expect(build.success && !build.rom.empty(), "assembly source should build for the console core");
	ok &= expect(build.addressToSourceLine.at(0) == 2 && build.sourceLineToAddress.at(4) == 16,
		"build debug metadata should map ROM addresses to original source lines");
	const auto macroBuild = console::buildAssemblySource("start:\n    %newline r0\n    hlt\n");
	ok &= expect(macroBuild.success && macroBuild.addressToSourceLine.at(0) == 2 &&
		macroBuild.addressToSourceLine.at(8) == 2 && macroBuild.addressToSourceLine.at(16) == 3,
		"macro-expanded instructions should map back to the invocation line");
	console::ConsoleMachine assembledMachine(console::pocketProfile());
	ok &= expect(assembledMachine.loadRom(build.rom, build.entryPoint, error), "assembled ROM should load");
	assembledMachine.run();
	assembledMachine.runForCycles(100);
	ok &= expect(assembledMachine.output().find('H') != std::string::npos, "assembled program should execute");

	const auto audioBuild = console::buildAssemblySource(
		"start:\n"
		"    movi r0, 0\n    stbi r0, 0xFF40\n"
		"    movi r0, 1\n    stbi r0, 0xFF41\n"
		"    movi r0, 0xB8\n    stbi r0, 0xFF42\n"
		"    movi r0, 1\n    stbi r0, 0xFF43\n"
		"    movi r0, 255\n    stbi r0, 0xFF44\n"
		"loop:\n    jmp loop\n");
	console::ConsoleMachine audioMachine(console::pocketProfile());
	ok &= expect(audioBuild.success && audioMachine.loadRom(audioBuild.rom, audioBuild.entryPoint, error),
		"audio test program should load");
	audioMachine.run();
	audioMachine.runForCycles(20'000);
	ok &= expect(!audioMachine.drainAudioSamples().empty(), "enabled channel should generate deterministic samples");

	if (ok) std::cout << "All console core tests passed.\n";
	return ok ? 0 : 1;
}
