#include "disassembler.h"

#include <iomanip>
#include <map>
#include <sstream>

namespace console {

std::string disassemble(uint64_t instruction) {
	const uint16_t opcode = static_cast<uint16_t>((instruction >> 48) & 0xFFFF);
	const uint8_t rx = static_cast<uint8_t>((instruction >> 40) & 0xFF);
	const uint8_t ry = static_cast<uint8_t>((instruction >> 32) & 0xFF);
	const uint8_t rz = static_cast<uint8_t>((instruction >> 24) & 0xFF);
	const uint16_t special = static_cast<uint16_t>(instruction & 0xFFFF);
	static const std::map<uint16_t, std::string> names = {
		{0x0000,"movi"},{0x0001,"mov"},{0x0002,"movc"},{0x0003,"add"},{0x0004,"sub"},
		{0x0005,"shl"},{0x0006,"shr"},{0x0007,"jmp"},{0x0008,"jz"},{0x0009,"jnz"},
		{0x000A,"je"},{0x000B,"jne"},{0x000C,"hlt"},{0x000D,"out"},{0x000E,"ld"},
		{0x000F,"ldi"},{0x0010,"st"},{0x0011,"sti"},{0x0012,"mul"},{0x0013,"div"},
		{0x0014,"mod"},{0x0015,"and"},{0x0016,"or"},{0x0017,"xor"},{0x0018,"not"},
		{0x0019,"jlt"},{0x001A,"jle"},{0x001B,"jgt"},{0x001C,"jge"},{0x001D,"push"},
		{0x001E,"pop"},{0x001F,"call"},{0x0020,"ret"},{0x0021,"outn"},{0x0022,"outs"},
		{0x0023,"ldb"},{0x0024,"stb"},{0x0025,"ldbi"},{0x0026,"stbi"},{0x0027,"ldbr"},
		{0x0028,"ldbri"},{0x0029,"ldwr"},{0x002A,"ldwri"},{0x002B,"in"},{0x002C,"inkey"},
	};
	const auto found = names.find(opcode);
	if (found == names.end()) {
		std::ostringstream unknown;
		unknown << ".word-opcode 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << opcode;
		return unknown.str();
	}

	std::ostringstream text;
	text << found->second;
	auto reg = [&text](uint8_t value) { text << " r" << static_cast<unsigned>(value); };
	auto separator = [&text]() { text << ','; };
	auto immediate = [&text](uint16_t value) {
		text << " 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << value;
	};

	switch (opcode) {
	case 0x000C: case 0x0020: break;
	case 0x0007: case 0x001F: immediate(special); break;
	case 0x000D: case 0x001D: case 0x001E: case 0x0021: case 0x0022: case 0x002B: case 0x002C:
		reg(rx); break;
	case 0x0000: case 0x000F: case 0x0011: case 0x0025: case 0x0026: case 0x0028: case 0x002A:
		reg(rx); separator(); immediate(special); break;
	case 0x0008: case 0x0009:
		reg(rx); separator(); immediate(special); break;
	case 0x000A: case 0x000B: case 0x0019: case 0x001A: case 0x001B: case 0x001C:
		reg(rx); separator(); reg(ry); separator(); immediate(special); break;
	case 0x0003: case 0x0004: case 0x0005: case 0x0006: case 0x0012: case 0x0013:
	case 0x0014: case 0x0015: case 0x0016: case 0x0017:
		reg(rx); separator(); reg(ry); separator(); reg(rz); break;
	default:
		reg(rx); separator(); reg(ry); break;
	}
	return text.str();
}

} // namespace console
