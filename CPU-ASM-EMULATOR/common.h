#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// CPU-ASM-EMULATOR CONFIG
inline constexpr int REGISTER_COUNT = 32; // Number of registers in the CPU emulator
inline constexpr int REGISTER_SIZE = 16; // Size of each register in bits
inline constexpr int INSTRUCTION_SIZE_BYTES = 8; // Each encoded instruction is 64 bits.
inline constexpr int ADDRESS_SPACE_SIZE = 65536; // 16-bit addresses can point at 64 KiB.

// Global variables to track modes and file paths

extern bool ARG_verboseMode; // Global variable to track verbose mode TODO: make this false by default, but for testing purposes, it's set to true for now
extern bool ARG_asmMode; // Global variable to track assembly mode (so if we need to asmbl)
extern const char* ARG_asmFilePath; // Non-owning pointer to the assembly file path argument
extern bool ARG_outbin; // Global variable to track if the user wants to output the assembled binary
extern bool ARG_emulate; // Global variable to track if the user wants to emulate the assembled binary
extern bool ARG_nodefaults; // Global variable to track if the user wants to skip loading default includes (macros)