#include "assembler.h"
#include "defaultincludes.h"

// Assembly variables

std::map <std::string, uint16_t> labels = {}; // Map to store labels and their corresponding ROM byte addresses.
std::vector<uint64_t> outputBinary; // Vector to store the output binary instructions (bits)
std::map <std::string, int16_t> registerNames = {}; // Map to store register names and their corresponding register numbers (e.g., "R0" -> 0000000000000000, "R1" -> 0000000000000001, ..., "R31" -> ...)
std::string asmFileContent = ""; // Variable to store the content of the assembly file as a string

std::vector<RomChunk> outputRom;
uint16_t outputRomAddress = 0; // Tracks the ROM byte address while emitting chunks in the second pass.
uint16_t entryPoint = 0; // ROM byte address where emulation starts.
bool resetVectorEnabled = false; // True when `.reset label` should write a hardware-style reset vector.
uint16_t resetVectorAddress = 0; // ROM byte address written into reset vector bytes 0 and 1.

// Stores one macro definition after parsing `%macro name param...`.
// Body lines are kept as source text, then expanded before labels/instructions are processed.
struct Macro {
	std::vector<std::string> params;
	std::vector<std::string> body;
};

std::map<std::string, Macro> macros;

enum class EncodingKind { // Enum to represent the kind of instruction encoding based on the operand types
	RR,   // opcode | rx | ry | 0  | mode | 0
	RRR,  // opcode | rx | ry | rz | mode | 0
	R,    // opcode | rx | 0  | 0  | mode | 0
	RI,   // opcode | rx | 0  | 0  | mode | imm
	J,    // opcode | 0  | 0  | 0  | mode | label
	JC,   // opcode | rx | 0  | 0  | mode | label (conditional jump based on register value)
	JL,   // opcode | rx | ry | 0  | mode | label (conditional jump based on register differences)
	None, // No specific encoding (used for instructions without operands)
};

struct ParsedInstruction { // Struct to represent a parsed instruction, including its mnemonic and operands
	std::string mnemonic;
	std::vector<std::string> operands;
};

enum class OperandKind { // Enum to represent the kind of operand in an instruction
	None,
	Register,
	Immediate,
	Label
};

struct InstructionDef { // Struct to represent the definition of an instruction, including its opcode, operand kinds, and encoding type
	uint16_t opcode;
	std::vector<OperandKind> operands;
	EncodingKind encoding;
};

const std::map<std::string, InstructionDef> instructionSet = { // Map to define the instruction set, associating mnemonics with their corresponding opcodes, operand kinds, and encoding types
	{"mov",  {0x0001, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"movc", {0x0002, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"add",  {0x0003, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"sub",  {0x0004, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"shl",  {0x0005, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"shr",  {0x0006, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"movi", {0x0000, {OperandKind::Register, OperandKind::Immediate}, EncodingKind::RI}},
	{"jmp",  {0x0007, {OperandKind::Label}, EncodingKind::J}},
	{"jz",   {0x0008, {OperandKind::Register, OperandKind::Label}, EncodingKind::JC}},
	{"jnz",  {0x0009, {OperandKind::Register, OperandKind::Label}, EncodingKind::JC}},
	{"je",   {0x000a, {OperandKind::Register, OperandKind::Register, OperandKind::Label}, EncodingKind::JL}},
	{"jne",  {0x000b, {OperandKind::Register, OperandKind::Register, OperandKind::Label}, EncodingKind::JL}},
	{"hlt",  {0x000c, {}, EncodingKind::None}},
	{"out",  {0x000d, {OperandKind::Register}, EncodingKind::R}},
	{"ld",   {0x000e, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"ldi",  {0x000f, {OperandKind::Register, OperandKind::Immediate}, EncodingKind::RI}},
	{"st",   {0x0010, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"sti",  {0x0011, {OperandKind::Register, OperandKind::Immediate}, EncodingKind::RI}},
	{"mul",  {0x0012, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"div",  {0x0013, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"mod",  {0x0014, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"and",  {0x0015, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"or",   {0x0016, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"xor",  {0x0017, {OperandKind::Register, OperandKind::Register, OperandKind::Register}, EncodingKind::RRR}},
	{"not",  {0x0018, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"jlt",  {0x0019, {OperandKind::Register, OperandKind::Register, OperandKind::Label}, EncodingKind::JL}},
	{"jle",  {0x001a, {OperandKind::Register, OperandKind::Register, OperandKind::Label}, EncodingKind::JL}},
	{"jgt",  {0x001b, {OperandKind::Register, OperandKind::Register, OperandKind::Label}, EncodingKind::JL}},
	{"jge",  {0x001c, {OperandKind::Register, OperandKind::Register, OperandKind::Label}, EncodingKind::JL}},
	{"push", {0x001d, {OperandKind::Register}, EncodingKind::R}},
	{"pop",  {0x001e, {OperandKind::Register}, EncodingKind::R}},
	{"call", {0x001f, {OperandKind::Label}, EncodingKind::J}},
	{"ret",  {0x0020, {}, EncodingKind::None}},
	{"outn", {0x0021, {OperandKind::Register}, EncodingKind::R}},
	{"outs", {0x0022, {OperandKind::Register}, EncodingKind::R}},
	{"ldb",  {0x0023, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"stb",  {0x0024, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"ldbi", {0x0025, {OperandKind::Register, OperandKind::Immediate}, EncodingKind::RI}},
	{"stbi", {0x0026, {OperandKind::Register, OperandKind::Immediate}, EncodingKind::RI}},
	{"ldbr", {0x0027, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"ldbri",{0x0028, {OperandKind::Register, OperandKind::Immediate}, EncodingKind::RI}},
	{"ldwr", {0x0029, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"ldwri",{0x002a, {OperandKind::Register, OperandKind::Immediate}, EncodingKind::RI}},
};

// Function to initialize register names and their corresponding register numbers
void initializeRegisterNames() {
	// Create register name keys "R0".."R31" and map to their numeric index.
	// This map is used later by getRegisterNumber / encodeOperand to resolve register operands.
	for (int i = 0; i < REGISTER_COUNT; ++i) {
		std::string regName = "R" + std::to_string(i);
		registerNames[regName] = i; // Map register name to its corresponding register number
	}
}

// Function to find register number from the register names map (case-insensitive)
int16_t getRegisterNumber(const std::string& regName) {
	std::string regNameLower = regName;
	// Convert the register name to uppercase for case-insensitive comparison.
	// The registerNames map stores keys like "R0", so normalising prevents "r0" mismatches.
	for (char& c : regNameLower) {
		c = std::toupper(c);
	}
	// Lookup the normalized name in the register map.
	auto it = registerNames.find(regNameLower);
	if (it != registerNames.end()) {
		return it->second; // Return the register number if found
	}
	else {
		return -1; // Return -1 if the register name is not found in the map
	}
}

// Function to get the binary opcode for a given instruction mnemonic
uint16_t encodeOperand(const std::string& token, OperandKind kind) {
	// Encode a single operand depending on its declared kind:
	// - Register: resolve register name to index (0..REGISTER_COUNT-1)
	// - Immediate: convert decimal numeric string to uint16_t via std::stoi
	// - Label: lookup label -> ROM byte address (from first pass)
	// - None: zero
	switch (kind) {
	case OperandKind::Register: {
		int16_t reg = getRegisterNumber(token);
		if (reg == -1) throw std::runtime_error("Invalid register: " + token);
		// Register fields are 8 bits wide in the instruction format.
		return static_cast<uint16_t>(reg);
	}
	case OperandKind::Immediate: {
		// Convert immediate decimal string to integer.
		auto it = labels.find(token); // I dont really understand what this code dose, but ai is great at writing code, so i will just assume this is correct and it allows us to use labels as immediates, which is a nice feature to have.
		if (it != labels.end()) {
			return static_cast<uint16_t>(it->second);
		}

		// Note: std::stoi throws if token is not a valid integer; caller will receive exception.
		int value = std::stoi(token);
		return static_cast<uint16_t>(value);
	}
	case OperandKind::Label: {
		// Labels are resolved during the first pass and stored in `labels`.
		// The label value is a ROM byte address.
		auto it = labels.find(token);
		if (it == labels.end()) throw std::runtime_error("Unknown label: " + token);
		return static_cast<uint16_t>(it->second);
	}
	case OperandKind::None:
		return 0;
	}
	// Fallback: signal an encoding error for unknown OperandKind.
	throw std::runtime_error("Unhandled operand kind");
}

// Function to encode a parsed instruction into a 64-bit binary instruction based on the instruction definitions and operand encoding
uint64_t encodeInstruction(const ParsedInstruction& ins) {
	auto it = instructionSet.find(ins.mnemonic);
	if (it == instructionSet.end()) {
		throw std::runtime_error("Unknown instruction: " + ins.mnemonic);
	}

	const InstructionDef& def = it->second;

	if (ins.operands.size() != def.operands.size()) {
		throw std::runtime_error("Wrong operand count for " + ins.mnemonic);
	}

	uint16_t op = def.opcode;
	uint8_t rx = 0, ry = 0, rz = 0, mode = 0;
	uint16_t sp = 0;

	switch (def.encoding) {
	case EncodingKind::RR:
		rx = static_cast<uint8_t>(encodeOperand(ins.operands[0], def.operands[0])); // Rx
		ry = static_cast<uint8_t>(encodeOperand(ins.operands[1], def.operands[1])); // Ry
		break;

	case EncodingKind::RRR:
		rx = static_cast<uint8_t>(encodeOperand(ins.operands[0], def.operands[0])); // Rx
		ry = static_cast<uint8_t>(encodeOperand(ins.operands[1], def.operands[1])); // Ry
		rz = static_cast<uint8_t>(encodeOperand(ins.operands[2], def.operands[2])); // Rz
		break;

	case EncodingKind::R:
		rx = static_cast<uint8_t>(encodeOperand(ins.operands[0], def.operands[0])); // Rx
		break;

	case EncodingKind::RI:
		rx = static_cast<uint8_t>(encodeOperand(ins.operands[0], def.operands[0])); // Rx
		sp = encodeOperand(ins.operands[1], def.operands[1]); // imm in special field
		break;

	case EncodingKind::J:
		sp = encodeOperand(ins.operands[0], def.operands[0]); // label in special field
		break;

	case EncodingKind::JC:
		rx = static_cast<uint8_t>(encodeOperand(ins.operands[0], def.operands[0])); // rx
		sp = encodeOperand(ins.operands[1], def.operands[1]); // label in special field
		break;

	case EncodingKind::JL:
		rx = static_cast<uint8_t>(encodeOperand(ins.operands[0], def.operands[0])); // rx
		ry = static_cast<uint8_t>(encodeOperand(ins.operands[1], def.operands[1])); // ry
		sp = encodeOperand(ins.operands[2], def.operands[2]); // label in special field
		break;

	case EncodingKind::None:
		// No operands to encode, all fields remain zero.
		break;
	}

	return (static_cast<uint64_t>(op) << 48) |
		(static_cast<uint64_t>(rx) << 40) |
		(static_cast<uint64_t>(ry) << 32) |
		(static_cast<uint64_t>(rz) << 24) |
		(static_cast<uint64_t>(mode) << 16) |
		static_cast<uint64_t>(sp);
}

// Function to trim whitespace from a string (used for parsing assembly lines)
std::string trim(const std::string& str) {
	// Find first/last non-whitespace characters and return substring between them.
	size_t first = str.find_first_not_of(" \t\n\r");
	size_t last = str.find_last_not_of(" \t\n\r");
	if (first == std::string::npos || last == std::string::npos) {
		return ""; // Return an empty string if the input string is all whitespace
	}
	return str.substr(first, last - first + 1); // Return the trimmed string
}

bool loadDefaultIncludes() {
	if (ARG_verboseMode) {
		std::cout << "Loaded built-in default includes.\n";
	}

	asmFileContent = std::string(DEFAULT_INCLUDES_ASM) + "\n" + asmFileContent;
	return true;
}

// Function to parse a line of assembly code into a ParsedInstruction struct, including the mnemonic and operands
ParsedInstruction parseLine(const std::string& rawLine) {
	std::string line = rawLine;

	// Strip inline comments starting with ';'
	size_t commentPos = line.find(';');
	if (commentPos != std::string::npos) {
		line = line.substr(0, commentPos);
	}

	// Copy input and normalize comma separators to spaces so >> extraction works uniformly.
	for (char& c : line) {
		if (c == ',') c = ' ';
	}

	std::istringstream ss(line);
	ParsedInstruction result;
	// First token is the mnemonic
	ss >> result.mnemonic;

	// The remaining tokens are operands (separated by spaces after comma->space replacement)
	std::string operand;
	while (ss >> operand) {
		result.operands.push_back(operand);
	}

	// Normalize mnemonic to lower-case to match `instructionSet` keys.
	for (char& c : result.mnemonic) {
		c = std::tolower(c);
	}

	return result;
}

// Function to find and map labels in the assembly code (first pass of the assembler)
bool processLabels() {
	std::istringstream iss(asmFileContent);
	std::string line;
	uint16_t romAddress = 0; // Byte address of the next instruction in ROM.
	while (std::getline(iss, line)) {
		line = trim(line); // Trim whitespace from the line
		if (line.empty()) {
			continue; // Skip empty lines
		}
		if (line.back() == ':') { // Check if the line is a label (ends with ':')
			// Extract the label name by removing the trailing ':' and store the current ROM byte address.
			std::string labelName = line.substr(0, line.size() - 1); // Extract the label name by removing the trailing ':'
			labels[labelName] = romAddress; // Map the label name to the current ROM byte address.
		} else if (line.rfind(".byte", 0) == 0) {
			// For now, each .byte line emits exactly one byte into ROM.
			romAddress = static_cast<uint16_t>(romAddress + 1);
		} else if (line.rfind(".asciiz", 0) == 0) {
			// .asciiz "string" emits the raw bytes of the string followed by a null terminator.
			size_t firstQuote = line.find('"');
			size_t lastQuote = line.find_last_of('"');

			if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote) {
				romAddress = static_cast<uint16_t>(romAddress + (lastQuote - firstQuote - 1) + 1);
			}
		} else if (line.rfind(".ascii", 0) == 0) {
			// .ascii "string" emits the raw bytes of the string without a null terminator.
			size_t firstQuote = line.find('"');
			size_t lastQuote = line.find_last_of('"');

			if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote) {
				romAddress = static_cast<uint16_t>(romAddress + (lastQuote - firstQuote - 1));
			}

		} else if (line.rfind(".word", 0) == 0) {
			// .word emits one 16-bit value into ROM as two bytes.
			romAddress = static_cast<uint16_t>(romAddress + 2);

		} else if (line.rfind(".space", 0) == 0) {
			// .space N reserves N bytes in ROM, so we need to parse the operand to know how much to increment the ROM address.
			ParsedInstruction ins = parseLine(line);

			if (ins.operands.size() == 1) {
				int count = std::stoi(ins.operands[0]);
				if (count >= 0) {
					romAddress = static_cast<uint16_t>(romAddress + count);
				}
			}

		} else if (line.rfind(".align", 0) == 0) {
			// .align N advances to the next ROM address divisible by N.
			ParsedInstruction ins = parseLine(line);

			if (ins.operands.size() == 1) {
				int alignment = std::stoi(ins.operands[0]);
				if (alignment > 0) {
					while (romAddress % alignment != 0) {
						romAddress = static_cast<uint16_t>(romAddress + 1);
					}
				}
			}

		} else if (line.rfind(".org", 0) == 0) {
			// .org ADDRESS sets the current ROM byte address to the specified value.	
			ParsedInstruction ins = parseLine(line);

			if (ins.operands.size() == 1) {
				int target = std::stoi(ins.operands[0]);

				if (target >= romAddress) {
					romAddress = static_cast<uint16_t>(target);
				}
			}

		} else if (line.rfind(".entry", 0) == 0) {
			// no ROM bytes

		} else if (line.rfind(".reset", 0) == 0) {
			// .reset writes the target address into ROM bytes 0 and 1 later, but emits no bytes here.

		} else {
			romAddress = static_cast<uint16_t>(romAddress + INSTRUCTION_SIZE_BYTES); // Increment by one 64-bit instruction.
		}
	}
	return true; // Return true if labels are processed successfully, false otherwise
}

// Function to convert assembly source lines into ROM chunks.
// Instructions become 64-bit chunks, while data directives like `.byte` become raw byte chunks.
bool processInstruction() {
	std::istringstream iss(asmFileContent);
	std::string line;

	// Second pass: read each non-label line and emit the matching ROM chunk.
	while (std::getline(iss, line)) {
		line = trim(line);
		// Skip empty lines and label definitions (already handled in first pass).
		if (line.empty() || line.back() == ':') {
			continue;
		}

		try {
			ParsedInstruction ins = parseLine(line);

			if (ins.mnemonic == ".byte") { // Handle .byte directive to emit a single byte into ROM.
				if (ins.operands.size() != 1) {
					throw std::runtime_error(".byte requires exactly one value");
				}

				int value = std::stoi(ins.operands[0]);
				if (value < 0 || value > 255) {
					throw std::runtime_error(".byte value must be between 0 and 255");
				}

				outputRom.push_back({ false, 0, static_cast<uint8_t>(value) });
				outputRomAddress = static_cast<uint16_t>(outputRomAddress + 1);
				continue;

			} else if (ins.mnemonic == ".asciiz") { // Handle .asciiz directive to emit the raw bytes of a string followed by a null terminator into ROM.
				size_t firstQuote = line.find('"');
				size_t lastQuote = line.find_last_of('"');

				if (firstQuote == std::string::npos || lastQuote == std::string::npos || lastQuote <= firstQuote) {
					throw std::runtime_error(".asciiz requires quoted text");
				}

				std::string text = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);

				for (char c : text) {
					outputRom.push_back({ false, 0, static_cast<uint8_t>(c) });
					outputRomAddress = static_cast<uint16_t>(outputRomAddress + 1);
				}

				outputRom.push_back({ false, 0, 0 });
				outputRomAddress = static_cast<uint16_t>(outputRomAddress + 1);
				continue;
			
			} else if (ins.mnemonic == ".ascii") { // Handle .ascii directive to emit the raw bytes of a string into ROM.
				size_t firstQuote = line.find('"');
				size_t lastQuote = line.find_last_of('"');

				if (firstQuote == std::string::npos || lastQuote == std::string::npos || lastQuote <= firstQuote) {
					throw std::runtime_error(".ascii requires quoted text");
				}

				std::string text = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);

				for (char c : text) {
					outputRom.push_back({ false, 0, static_cast<uint8_t>(c) });
					outputRomAddress = static_cast<uint16_t>(outputRomAddress + 1);
				}

				continue;

			} else if (ins.mnemonic == ".word") { // Handle .word directive to emit a 16-bit value into ROM as two bytes (big-endian).
				if (ins.operands.size() != 1) {
					throw std::runtime_error(".word requires exactly one value");
				}

				int value = std::stoi(ins.operands[0]);

				if (value < 0 || value > 65535) {
					throw std::runtime_error(".word value must be between 0 and 65535");
				}

				outputRom.push_back({ false, 0, static_cast<uint8_t>((value >> 8) & 0xFF) });
				outputRom.push_back({ false, 0, static_cast<uint8_t>(value & 0xFF) });
				outputRomAddress = static_cast<uint16_t>(outputRomAddress + 2);
				continue;

			} else if (ins.mnemonic == ".space") { // Handle .space directive to reserve a specified number of bytes in ROM (emit zero bytes).
				if (ins.operands.size() != 1) {
					throw std::runtime_error(".space requires exactly one count");
				}

				int count = std::stoi(ins.operands[0]);

				if (count < 0 || count > 65535) {
					throw std::runtime_error(".space count must be between 0 and 65535");
				}

				for (int i = 0; i < count; ++i) {
					outputRom.push_back({ false, 0, 0 });
					outputRomAddress = static_cast<uint16_t>(outputRomAddress + 1);
				}

				continue;

			} else if (ins.mnemonic == ".align") { // Emit zero bytes until the current ROM address is divisible by the requested alignment.
				if (ins.operands.size() != 1) {
					throw std::runtime_error(".align requires exactly one alignment");
				}

				int alignment = std::stoi(ins.operands[0]);

				if (alignment <= 0 || alignment > 65535) {
					throw std::runtime_error(".align value must be between 1 and 65535");
				}

				while (outputRomAddress % alignment != 0) {
					outputRom.push_back({ false, 0, 0 });
					outputRomAddress = static_cast<uint16_t>(outputRomAddress + 1);
				}

				continue;

			} else if (ins.mnemonic == ".org") { // Move the current ROM address forward to the specified target, emitting zero bytes as needed. .org cannot move backwards.
				if (ins.operands.size() != 1) {
					throw std::runtime_error(".org requires exactly one address");
				}

				int target = std::stoi(ins.operands[0]);

				if (target < 0 || target > 65535) {
					throw std::runtime_error(".org address must be between 0 and 65535");
				}

				if (target < outputRomAddress) {
					throw std::runtime_error(".org cannot move backwards");
				}

				while (outputRomAddress < target) {
					outputRom.push_back({ false, 0, 0 });
					outputRomAddress = static_cast<uint16_t>(outputRomAddress + 1);
				}

				continue;
			
			} else if (ins.mnemonic == ".entry") { // Handle .entry directive to set the entry point of the program (the ROM address where emulation starts).
				if (ins.operands.size() != 1) {
					throw std::runtime_error(".entry requires exactly one label");
				}

				auto it = labels.find(ins.operands[0]);
				if (it == labels.end()) {
					throw std::runtime_error("Unknown entry label: " + ins.operands[0]);
				}

				entryPoint = it->second;
				continue;

			} else if (ins.mnemonic == ".reset") { // Handle .reset directive to write a hardware-style reset vector at ROM address 0.
				if (ins.operands.size() != 1) {
					throw std::runtime_error(".reset requires exactly one label");
				}

				auto it = labels.find(ins.operands[0]);
				if (it == labels.end()) {
					throw std::runtime_error("Unknown reset label: " + ins.operands[0]);
				}

				resetVectorEnabled = true;
				resetVectorAddress = it->second;
				entryPoint = it->second; // Keep .entry-style startup in sync for tools that still look at entryPoint.
				continue;

			}

			uint64_t encoded = encodeInstruction(ins);
			// Keep instruction-only output for verbose dumps and --outbin.
			outputBinary.push_back(encoded);
			outputRom.push_back({ true, encoded, 0 });
			outputRomAddress = static_cast<uint16_t>(outputRomAddress + INSTRUCTION_SIZE_BYTES);
		}
		catch (const std::exception& e) {
			// Propagate error details including the source line for easier debugging.
			std::cout << "Error: " << e.what() << " in line: " << line << "\n";
			return false;
		}
	}

	return true;
}

// Function to process macros in the assembly code
bool processMacros() {
	std::istringstream iss(asmFileContent);
	std::ostringstream expanded;
	std::string line;

	bool inMacro = false;
	std::string currentMacroName;
	Macro currentMacro;

	while (std::getline(iss, line)) {
		line = trim(line);

		if (line.empty()) {
			continue;
		}

		ParsedInstruction parsed = parseLine(line);

		if (parsed.mnemonic == "%macro") {
			if (inMacro) {
				std::cout << "Nested macro definitions are not supported.\n";
				return false;
			}
			if (parsed.operands.empty()) {
				std::cout << "Macro definition requires a name.\n";
				return false;
			}

			inMacro = true;
			currentMacroName = parsed.operands[0];
			for (char& c : currentMacroName) {
				c = std::tolower(c);
			}
			currentMacro = Macro{};

			// Remaining `%macro` operands are parameter names used as `{param}` placeholders.
			for (size_t i = 1; i < parsed.operands.size(); ++i) {
				currentMacro.params.push_back(parsed.operands[i]);
			}

			continue;
		}

		if (parsed.mnemonic == "%endmacro") {
			if (!inMacro) {
				std::cout << "%endmacro found without a matching %macro.\n";
				return false;
			}

			macros[currentMacroName] = currentMacro;
			inMacro = false;
			continue;
		}

		if (inMacro) {
			// Do not parse macro body instructions yet. They may contain placeholders
			// that only become valid assembly once the macro is invoked.
			currentMacro.body.push_back(line);
			continue;
		}

		if (!line.empty() && line[0] == '%') {
			// A non-definition `%name ...` line invokes a previously defined macro.
			std::string macroName = parsed.mnemonic.substr(1);

			auto it = macros.find(macroName);
			if (it == macros.end()) {
				std::cout << "Unknown macro: " << macroName << "\n";
				return false;
			}

			const Macro& macro = it->second;

			if (parsed.operands.size() != macro.params.size()) {
				std::cout << "Wrong argument count for macro: " << macroName << "\n";
				return false;
			}

			for (std::string bodyLine : macro.body) {
				for (size_t i = 0; i < macro.params.size(); ++i) {
					std::string needle = "{" + macro.params[i] + "}";
					size_t pos = 0;

					// Replace every `{param}` placeholder with the corresponding call argument.
					while ((pos = bodyLine.find(needle, pos)) != std::string::npos) {
						bodyLine.replace(pos, needle.size(), parsed.operands[i]);
						pos += parsed.operands[i].size();
					}
				}

				expanded << bodyLine << "\n";
			}

			continue;
		}

		expanded << line << "\n";
	}

	if (inMacro) {
		std::cout << "Unterminated macro definition: " << currentMacroName << "\n";
		return false;
	}

	asmFileContent = expanded.str();
	return true;
}


// Assembler function to convert assembly code into binary instructions (64-bit)
bool assemble() {
	if (asmFileContent == "") return false; // Return false if the assembly file content is empty

	// Remove ';' comments and trim whitespace from the assembly file content before processing
	std::istringstream iss(asmFileContent);
	std::string line;
	std::string cleanedContent;
	while (std::getline(iss, line)) {
		size_t commentPos = line.find(';');
		if (commentPos != std::string::npos) {
			line = line.substr(0, commentPos);
		}
		cleanedContent += trim(line) + "\n";
	}
	asmFileContent = cleanedContent;

	// First pass: Process macros
	if (ARG_verboseMode) {
		std::cout << "Processing macros in the assembly code...\n";
	}
	if (!processMacros()) {
		std::cout << "Error: Failed to process macros in the assembly code.\n";
		return false; // Return false if macro processing fails
	}

	// Second pass: Process labels and store their corresponding line numbers (Program counter position) in the labels map
	if (asmFileContent == "") {
		std::cout << "Error: Assembly file content is empty after trimming whitespace.\n";
		return false; // Return false if the assembly file content is empty
	}

	labels.clear();
	outputBinary.clear();
	outputRom.clear();
	outputRomAddress = 0;
	entryPoint = 0;
	resetVectorEnabled = false;
	resetVectorAddress = 0;

	if (processLabels()) {
		if (ARG_verboseMode) {
			std::cout << "Labels processed successfully. Labels found:\n";
			for (const auto& label : labels) {
				std::cout << "Label: " << label.first << ", ROM Address: " << label.second << "\n";
			}
		}
	}
	else
	{
		std::cout << "Error: Failed to process labels in the assembly code.\n";
		return false; // Return false if label processing fails
	}

	// Second pass: Convert assembly instructions into binary instructions (64-bit) and store them in the outputBinary vector
	if (processInstruction()) {
		if (ARG_verboseMode) {
			std::cout << "Instructions processed successfully. Output binary instructions:\n";
			for (size_t i = 0; i < outputBinary.size(); ++i) {
				uint64_t instr = outputBinary[i];
				uint16_t op = (instr >> 48) & 0xFFFF;
				uint8_t rx = (instr >> 40) & 0xFF;
				uint8_t ry = (instr >> 32) & 0xFF;
				uint8_t rz = (instr >> 24) & 0xFF;
				uint8_t mode = (instr >> 16) & 0xFF;
				uint16_t sp = instr & 0xFFFF;

				std::cout
					<< "Instruction " << i << ": "
					<< std::bitset<16>(op) << " "
					<< std::bitset<8>(rx) << " "
					<< std::bitset<8>(ry) << " "
					<< std::bitset<8>(rz) << " "
					<< std::bitset<8>(mode) << " "
					<< std::bitset<16>(sp) << "\n";
			}
		}
		if (ARG_outbin) // If the user has requested to output the assembled binary, print the binary instructions without verbose mode details.
		{
			uint16_t address = 0;
			auto applyResetVectorByte = [](uint16_t byteAddress, uint8_t value) {
				// Show the final ROM image: .reset patches bytes 0 and 1 with the reset PC.
				if (resetVectorEnabled && byteAddress == 0) {
					return static_cast<uint8_t>((resetVectorAddress >> 8) & 0xFF);
				}
				if (resetVectorEnabled && byteAddress == 1) {
					return static_cast<uint8_t>(resetVectorAddress & 0xFF);
				}
				return value;
			};

			for (const RomChunk& chunk : outputRom) {
				if (chunk.isInstruction) {
					for (int byte = 0; byte < INSTRUCTION_SIZE_BYTES; ++byte) {
						int shift = (INSTRUCTION_SIZE_BYTES - 1 - byte) * 8;
						uint8_t value = static_cast<uint8_t>((chunk.instruction >> shift) & 0xFF);
						value = applyResetVectorByte(address, value);
						std::cout
							<< std::hex << std::uppercase
							<< std::setw(4) << std::setfill('0') << address
							<< ": "
							<< std::bitset<8>(value)
							<< std::dec << "\n";
						address++;
					}
				}
				else {
					// Raw ROM data bytes get addresses too, so labels/data layout is easy to inspect.
					uint8_t value = applyResetVectorByte(address, chunk.byte);
					std::cout
						<< std::hex << std::uppercase
						<< std::setw(4) << std::setfill('0') << address
						<< ": "
						<< std::bitset<8>(value)
						<< std::dec << "\n";
					address++;
				}
			}
		}
	}
	else
	{
		std::cout << "Error: Failed to process instructions in the assembly code.\n";
		return false; // Return false if instruction processing fails
	}

	return true; // Return true if assembly is successful, false otherwise
}


