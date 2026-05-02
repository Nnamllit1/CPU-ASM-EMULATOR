#pragma once

#include "common.h"

extern uint8_t instructionRom[ADDRESS_SPACE_SIZE]; // Instruction ROM, stored as real bytes.
extern size_t instructionRomSize; // Number of instruction ROM bytes loaded from the assembler output.
extern uint16_t PC; // Program Counter, as a byte address into instruction ROM.
extern std::array<uint16_t, REGISTER_COUNT> registers; // Array to represent the registers in the CPU emulator (16-bit registers)
extern uint8_t memory[ADDRESS_SPACE_SIZE]; // 64KB of RAM for the CPU emulator (addressable by 16-bit addresses)
extern uint16_t SP; // Stack Pointer
extern bool CPU_halted; // Tracks whether execution has stopped.

bool loadInstructionRom();
bool loadInstructionRomFromFile(const std::string& filePath);
uint64_t fetchInstruction(uint16_t addr);
uint16_t readInstructionRomWord(uint16_t addr);
void execute(uint64_t instr);
