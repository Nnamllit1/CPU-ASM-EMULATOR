#include "cpu.h"
#include "assembler.h"

// CPU emulator variables

uint8_t instructionRom[ADDRESS_SPACE_SIZE]; // Instruction ROM, stored as real bytes.
size_t instructionRomSize = 0; // Number of instruction ROM bytes loaded from the assembler output.
uint16_t PC = 0; // Program Counter, as a byte address into instruction ROM.
std::array<uint16_t, REGISTER_COUNT> registers; // Array to represent the registers in the CPU emulator (16-bit registers)
uint8_t memory[ADDRESS_SPACE_SIZE]; // 64KB of RAM for the CPU emulator (addressable by 16-bit addresses)
uint16_t SP = 0; // Stack Pointer
bool CPU_halted = false; // Tracks whether execution has stopped.

bool loadInstructionRom() {
	instructionRomSize = 0;
	std::fill(std::begin(instructionRom), std::end(instructionRom), 0);

	for (const RomChunk& chunk : outputRom) {
		if (chunk.isInstruction) {
			if (instructionRomSize + INSTRUCTION_SIZE_BYTES > ADDRESS_SPACE_SIZE) {
				std::cout << "Error: Instruction ROM is full.\n";
				return false;
			}

			// Store the 64-bit instruction in big-endian byte order.
			// This keeps ROM byte 0 equal to the first printed bit/byte of the opcode.
			for (int byte = 0; byte < INSTRUCTION_SIZE_BYTES; ++byte) {
				int shift = (INSTRUCTION_SIZE_BYTES - 1 - byte) * 8;
				instructionRom[instructionRomSize + byte] = static_cast<uint8_t>((chunk.instruction >> shift) & 0xFF);
			}

			instructionRomSize += INSTRUCTION_SIZE_BYTES;
		} else {
			if (instructionRomSize + 1 > ADDRESS_SPACE_SIZE) {
				std::cout << "Error: Instruction ROM is full.\n";
				return false;
			}

			instructionRom[instructionRomSize] = chunk.byte;
			instructionRomSize += 1;
		}
	}

	return true;
}

uint64_t fetchInstruction(uint16_t addr) {
	uint64_t instr = 0;

	// Fetch one 64-bit instruction from instruction ROM, one byte at a time.
	// The address wraps naturally through the 16-bit address space.
	for (int byte = 0; byte < INSTRUCTION_SIZE_BYTES; ++byte) {
		instr = (instr << 8) | instructionRom[(addr + byte) & 0xFFFF];
	}

	return instr;
}

// Function to read a 16-bit word from memory at the specified address (big-endian)
uint16_t readWord(uint16_t addr) {
	return (static_cast<uint16_t>(memory[addr]) << 8) |
		static_cast<uint16_t>(memory[(addr + 1) & 0xFFFF]);
}

// Function to write a 16-bit word to memory at the specified address (big-endian)
void writeWord(uint16_t addr, uint16_t value) {
	memory[addr] = (value >> 8) & 0xFF;
	memory[(addr + 1) & 0xFFFF] = value & 0xFF;
}

void execute(uint64_t instr) {
	uint16_t op = (instr >> 48) & 0xFFFF;
	uint8_t rx = (instr >> 40) & 0xFF;
	uint8_t ry = (instr >> 32) & 0xFF;
	uint8_t rz = (instr >> 24) & 0xFF;
	uint8_t mode = (instr >> 16) & 0xFF;
	uint16_t sp = instr & 0xFFFF;

	if (rx >= REGISTER_COUNT || ry >= REGISTER_COUNT || rz >= REGISTER_COUNT) {
		std::cout << "Register index out of range. rx=" << static_cast<int>(rx)
			<< " ry=" << static_cast<int>(ry)
			<< " rz=" << static_cast<int>(rz) << "\n";
		return;
	}

	if (ARG_verboseMode) {
		std::cout << "PC=" << PC
			<< " OP=" << op
			<< " RX=" << static_cast<int>(rx)
			<< " RY=" << static_cast<int>(ry)
			<< " RZ=" << static_cast<int>(rz)
			<< " MODE=" << static_cast<int>(mode)
			<< " SPECIAL=" << sp << "\n";
	}

	switch (op) {
	case 0x0000: // movi
		registers[rx] = sp;
		break;

	case 0x0001: // mov
		registers[rx] = registers[ry];
		break;

	case 0x0002: // movc
		registers[rx] = registers[ry];
		registers[ry] = 0;
		break;

	case 0x0003: // add
		registers[rx] = registers[ry] + registers[rz];
		break;

	case 0x0004: // sub
		registers[rx] = registers[ry] - registers[rz];
		break;

	case 0x0005: // shl
		registers[rx] = registers[ry] << registers[rz]; // TODO: Fix shift amount exceeding register size (e.g., shifting by 16 or more should result in zero)
		break;

	case 0x0006: // shr
		registers[rx] = registers[ry] >> registers[rz]; // TODO: Fix shift amount exceeding register size (e.g., shifting by 16 or more should result in zero)
		break;

	case 0x0007: // jmp
		PC = sp;
		return;

	case 0x0008: // jz
		if (registers[rx] == 0) {
			PC = sp;
			return;
		}
		break;

	case 0x0009: // jnz
		if (registers[rx] != 0) {
			PC = sp;
			return;
		}
		break;

	case 0x000a: // je
		if (registers[rx] == registers[ry]) {
			PC = sp;
			return;
		}
		break;

	case 0x000b: // jne
		if (registers[rx] != registers[ry]) {
			PC = sp;
			return;
		}
		break;

	case 0x000c: // hlt
		std::cout << "HLT encountered. Stopping execution.\n\n";
		CPU_halted = true;
		return;

	case 0x000d: // out
		if (ARG_verboseMode) { // If verbose mode is enabled, print the output of the register in binary format for better visibility of the register state.
			std::cout << "Output instruction: R" << static_cast<int>(rx) << " = " << std::bitset<16>(registers[rx]) << "\n";
		}
		// Print the output of the register in ascii character format if the value is a valid ASCII code (0-127)
		if (registers[rx] <= 127) {
			std::cout << static_cast<char>(registers[rx]);
		}
		break;

	case 0x000e: // ld: rX = memory[rY]
		registers[rx] = readWord(registers[ry]);
		break;

	case 0x000f: // ldi: rX = memory[imm]
		registers[rx] = readWord(sp);
		break;

	case 0x0010: // st: memory[rX] = rY
		writeWord(registers[rx], registers[ry]);
		break;

	case 0x0011: // sti: memory[imm] = rX
		writeWord(sp, registers[rx]);
		break;

	case 0x0012: // mul
		registers[rx] = registers[ry] * registers[rz];
		break;

	case 0x0013: // div
		if (registers[rz] == 0) {
			std::cout << "Division by zero. Stopping execution.\n";
			CPU_halted = true;
			return;
		}
		registers[rx] = registers[ry] / registers[rz];
		break;

	case 0x0014: // mod
		if (registers[rz] == 0) {
			std::cout << "Modulo by zero. Stopping execution.\n";
			CPU_halted = true;
			return;
		}
		registers[rx] = registers[ry] % registers[rz];
		break;

	case 0x0015: // and
		registers[rx] = registers[ry] & registers[rz];
		break;

	case 0x0016: // or
		registers[rx] = registers[ry] | registers[rz];
		break;

	case 0x0017: // xor
		registers[rx] = registers[ry] ^ registers[rz];
		break;

	case 0x0018: // not
		registers[rx] = ~registers[ry];
		break;

	case 0x0019: // jlt
		if (registers[rx] < registers[ry]) {
			PC = sp;
			return;
		}
		break;

	case 0x001a: // jle
		if (registers[rx] <= registers[ry]) {
			PC = sp;
			return;
		}
		break;

	case 0x001b: // jgt
		if (registers[rx] > registers[ry]) {
			PC = sp;
			return;
		}
		break;

	case 0x001c: // jge
		if (registers[rx] >= registers[ry]) {
			PC = sp;
			return;
		}
		break;

	case 0x001d: // push
		SP -= 2;
		writeWord(SP, registers[rx]);
		break;

	case 0x001e: // pop
		registers[rx] = readWord(SP);
		SP += 2;
		break;

	case 0x001f: // call
		SP -= 2;
		writeWord(SP, static_cast<uint16_t>(PC + INSTRUCTION_SIZE_BYTES)); // Push return address onto the stack.
		PC = sp; // Jump to the subroutine at the label address
		return;

	case 0x0020: // ret
		PC = readWord(SP); // Pop return address from the stack and jump back
		SP += 2;
		return;

	case 0x0021: // outn
		std::cout << registers[rx];
		break;

	case 0x0022: // outs
	{
		uint16_t addr = registers[rx];

		while (addr < instructionRomSize && instructionRom[addr] != 0) {
			std::cout << static_cast<char>(instructionRom[addr]);
			addr = static_cast<uint16_t>(addr + 1);
		}
		break;
	}

	case 0x0023: // ldb: rX = memory[rY] (byte)
		registers[rx] = memory[registers[ry]];
		break;

	case 0x0024: // stb: memory[rX] = rY (byte)
		memory[registers[rx]] = registers[ry] & 0xFF;
		break;

	case 0x0025: // ldbi: rX = memory[imm] (byte)
		registers[rx] = memory[sp];
		break;

	case 0x0026: // stbi: memory[imm] = rX (byte)
		memory[sp] = registers[rx] & 0xFF;
		break;

	case 0x0027: // ldbr: rX = instructionRom[rY] (byte)
		// Read one byte from instruction ROM using an address stored in a register.
		registers[rx] = instructionRom[registers[ry]];
		break;

	case 0x0028: // ldbri: rX = instructionRom[imm] (byte)
		// Read one byte from instruction ROM using the immediate/special field as the address.
		registers[rx] = instructionRom[sp];
		break;

	case 0x0029: // ldwr
		registers[rx] = (static_cast<uint16_t>(instructionRom[registers[ry]]) << 8) |
			instructionRom[(registers[ry] + 1) & 0xFFFF];
		break;

	case 0x002a: // ldwri
		registers[rx] = (static_cast<uint16_t>(instructionRom[sp]) << 8) |
			instructionRom[(sp + 1) & 0xFFFF];
		break;

	default:
		std::cout << "Unknown opcode: " << op << "\n";
		break;
	}

	PC = static_cast<uint16_t>(PC + INSTRUCTION_SIZE_BYTES);
}


