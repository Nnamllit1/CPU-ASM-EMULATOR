#pragma once

#include "common.h"

// Struct to represent a chunk of ROM, which can be either an instruction or a raw byte (e.g., from a .byte directive). This is used during the assembly process to keep track of what needs to be emitted into the final instruction ROM.
struct RomChunk {
	bool isInstruction;
	uint64_t instruction;
	uint8_t byte;
};

extern std::map <std::string, uint16_t> labels; // Map to store labels and their corresponding ROM byte addresses.
extern std::vector<uint64_t> outputBinary; // Vector to store the output binary instructions (bits)
extern std::map <std::string, int16_t> registerNames; // Map to store register names and their corresponding register numbers (e.g., "R0" -> 0000000000000000, "R1" -> 0000000000000001, ..., "R31" -> ...)
extern std::string asmFileContent; // Variable to store the content of the assembly file as a string
extern std::vector<RomChunk> outputRom;
extern uint16_t outputRomAddress; // Tracks the ROM byte address while emitting chunks in the second pass.
extern uint16_t entryPoint; // ROM byte address where emulation starts.
extern bool resetVectorEnabled; // True when `.reset label` should write a hardware-style reset vector.
extern uint16_t resetVectorAddress; // ROM byte address written into reset vector bytes 0 and 1.

void initializeRegisterNames();
bool loadDefaultIncludes();
bool assemble();
