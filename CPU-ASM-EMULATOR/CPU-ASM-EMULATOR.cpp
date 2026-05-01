#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <bitset>
#include <array>

#include "defaultincludes.h"

// CPU-ASM-EMULATOR CONFIG
const int16_t REGISTER_COUNT = 32; // Number of registers in the CPU emulator (only 16 bits max)
const int REGISTER_SIZE = 16; // Size of each register in bits


// Global variables to track modes and file paths

bool ARG_verboseMode = false; // Global variable to track verbose mode TODO: make this false by default, but for testing purposes, it's set to true for now
bool ARG_asmMode = false; // Global variable to track assembly mode (so if we need to asmbl)
const char* ARG_asmFilePath = nullptr; // Non-owning pointer to the assembly file path argument
bool ARG_outbin = false; // Global variable to track if the user wants to output the assembled binary
bool ARG_emulate = false; // Global variable to track if the user wants to emulate the assembled binary
bool ARG_nodefaults = false; // Global variable to track if the user wants to skip loading default includes (macros)


// Assembly variables

std::map <std::string, int16_t> labels = {}; // Map to store labels and their corresponding line numbers (Program counter position)
std::vector<uint64_t> outputBinary; // Vector to store the output binary instructions (bits)
std::map <std::string, int16_t> registerNames = {}; // Map to store register names and their corresponding register numbers (e.g., "R0" -> 0000000000000000, "R1" -> 0000000000000001, ..., "R31" -> ...)
std::string asmFileContent = ""; // Variable to store the content of the assembly file as a string

// Stores one macro definition after parsing `%macro name param...`.
// Body lines are kept as source text, then expanded before labels/instructions are processed.
struct Macro {
	std::vector<std::string> params;
	std::vector<std::string> body;
};

std::map<std::string, Macro> macros;

enum class EncodingKind { // Enum to represent the kind of instruction encoding based on the operand types
	RR,   // opcode | rx | ry | 0
	R,    // opcode | rx | 0  | 0
	RI,   // opcode | rx | 0  | imm
	J,    // opcode | 0  | 0  | label
	JC,   // opcode | rx | 0  | label (conditional jump based on register value)
	JL,   // opcode | rx | ry | label (conditional jump based on register differences)
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
	{"add",  {0x0003, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"sub",  {0x0004, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"shl",  {0x0005, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"shr",  {0x0006, {OperandKind::Register, OperandKind::Register}, EncodingKind::RR}},
	{"movi", {0x0000, {OperandKind::Register, OperandKind::Immediate}, EncodingKind::RI}},
	{"jmp",  {0x0007, {OperandKind::Label}, EncodingKind::J}},
	{"jz",   {0x0008, {OperandKind::Register, OperandKind::Label}, EncodingKind::JC}},
	{"jnz",  {0x0009, {OperandKind::Register, OperandKind::Label}, EncodingKind::JC}},
	{"je",   {0x000a, {OperandKind::Register, OperandKind::Register, OperandKind::Label}, EncodingKind::JL}},
	{"jne",  {0x000b, {OperandKind::Register, OperandKind::Register, OperandKind::Label}, EncodingKind::JL}},
	{"hlt",  {0x000c, {}, EncodingKind::None}},
	{"out",  {0x000d, {OperandKind::Register}, EncodingKind::R}}
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
	// - Label: lookup label -> instruction index (from first pass)
	// - None: zero
	switch (kind) {
	case OperandKind::Register: {
		int16_t reg = getRegisterNumber(token);
		if (reg == -1) throw std::runtime_error("Invalid register: " + token);
		// Registers are stored in lower 16 bits of their field.
		return static_cast<uint16_t>(reg);
	}
	case OperandKind::Immediate: {
		// Convert immediate decimal string to integer.
		// Note: std::stoi throws if token is not a valid integer; caller will receive exception.
		int value = std::stoi(token);
		return static_cast<uint16_t>(value);
	}
	case OperandKind::Label: {
		// Labels are resolved during the first pass and stored in `labels`.
		// The label value is the instruction index (program counter position).
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
	uint16_t a = 0, b = 0, c = 0;

	switch (def.encoding) {
	case EncodingKind::RR:
		a = encodeOperand(ins.operands[0], def.operands[0]); // Rx
		b = encodeOperand(ins.operands[1], def.operands[1]); // Ry
		break;

	case EncodingKind::R:
		a = encodeOperand(ins.operands[0], def.operands[0]); // Rx
		break;

	case EncodingKind::RI:
		a = encodeOperand(ins.operands[0], def.operands[0]); // Rx
		c = encodeOperand(ins.operands[1], def.operands[1]); // imm in special field
		break;

	case EncodingKind::J:
		c = encodeOperand(ins.operands[0], def.operands[0]); // label in special field
		break;

	case EncodingKind::JC:
		a = encodeOperand(ins.operands[0], def.operands[0]); // rx
		c = encodeOperand(ins.operands[1], def.operands[1]); // label in special field
		break;

	case EncodingKind::JL:
		a = encodeOperand(ins.operands[0], def.operands[0]); // rx
		b = encodeOperand(ins.operands[1], def.operands[1]); // ry
		c = encodeOperand(ins.operands[2], def.operands[2]); // label in special field
		break;

	case EncodingKind::None:
		// No operands to encode, all fields remain zero.
		break;
	}

	return (static_cast<uint64_t>(op) << 48) |
		(static_cast<uint64_t>(a) << 32) |
		(static_cast<uint64_t>(b) << 16) |
		static_cast<uint64_t>(c);
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
	int lineNumber = 0; // Line number to track the position of instructions for labels
	while (std::getline(iss, line)) {
		line = trim(line); // Trim whitespace from the line
		if (line.empty()) {
			continue; // Skip empty lines
		}
		if (line.back() == ':') { // Check if the line is a label (ends with ':')
			// Extract the label name by removing the trailing ':' and store the current instruction index.
			// Note: we use instruction index (lineNumber) so labels point to the correct PC location.
			std::string labelName = line.substr(0, line.size() - 1); // Extract the label name by removing the trailing ':'
			labels[labelName] = lineNumber; // Map the label name to the current line number (program counter position)
		} else {
			lineNumber++; // Increment line number for each instruction line (non-label)
		}
	}
	return true; // Return true if labels are processed successfully, false otherwise
}

// Function to convert an assembly instruction line into a binary instruction (64-bit) and store it in the outputBinary vector
bool processInstruction() {
	std::istringstream iss(asmFileContent);
	std::string line;

	// Second pass: read each non-label line, parse it and encode into binary.
	while (std::getline(iss, line)) {
		line = trim(line);
		// Skip empty lines and label definitions (already handled in first pass).
		if (line.empty() || line.back() == ':') {
			continue;
		}

		try {
			ParsedInstruction ins = parseLine(line);
			uint64_t encoded = encodeInstruction(ins);
			// Append the encoded 64-bit instruction to the output vector.
			outputBinary.push_back(encoded);
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

	if (processLabels()) {
		if (ARG_verboseMode) {
			std::cout << "Labels processed successfully. Labels found:\n";
			for (const auto& label : labels) {
				std::cout << "Label: " << label.first << ", Line Number: " << label.second << "\n";
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
				uint16_t rx = (instr >> 32) & 0xFFFF;
				uint16_t ry = (instr >> 16) & 0xFFFF;
				uint16_t sp = instr & 0xFFFF;

				std::cout
					<< "Instruction " << i << ": "
					<< std::bitset<16>(op) << " "
					<< std::bitset<16>(rx) << " "
					<< std::bitset<16>(ry) << " "
					<< std::bitset<16>(sp) << "\n";
			}
		}
		if (ARG_outbin) // If the user has requested to output the assembled binary, print the binary instructions without verbose mode details.
		{
			for (size_t i = 0; i < outputBinary.size(); ++i) {
				std::cout << std::bitset<64>(outputBinary[i]) << "\n";
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


// CPU emulator variables

std::vector<uint64_t> program; // Array to store the assembly program instructions (bits)
int PC = 0; // Program Counter
std::array<uint16_t, REGISTER_COUNT> registers; // Array to represent the registers in the CPU emulator (16-bit registers)

void execute(uint64_t instr) {
	uint16_t op = (instr >> 48) & 0xFFFF;
	uint16_t rx = (instr >> 32) & 0xFFFF;
	uint16_t ry = (instr >> 16) & 0xFFFF;
	uint16_t sp = instr & 0xFFFF;

	if (rx >= REGISTER_COUNT || ry >= REGISTER_COUNT) {
		std::cout << "Register index out of range. rx=" << rx << " ry=" << ry << "\n";
		return;
	}

	if (ARG_verboseMode) {
		std::cout << "PC=" << PC
			<< " OP=" << op
			<< " RX=" << rx
			<< " RY=" << ry
			<< " SP=" << sp << "\n";
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
		registers[rx] += registers[ry];
		break;

	case 0x0004: // sub
		registers[rx] -= registers[ry];
		break;

	case 0x0005: // shl
		registers[rx] <<= registers[ry]; // TODO: Fix shift amount exceeding register size (e.g., shifting by 16 or more should result in zero)
		break;

	case 0x0006: // shr
		registers[rx] >>= registers[ry]; // TODO: Fix shift amount exceeding register size (e.g., shifting by 16 or more should result in zero)
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
		PC = static_cast<int>(program.size());
		return;

	case 0x000d: // out
		if (ARG_verboseMode) { // If verbose mode is enabled, print the output of the register in binary format for better visibility of the register state.
			std::cout << "Output instruction: R" << rx << " = " << std::bitset<16>(registers[rx]) << "\n";
		}
		// Print the output of the register in ascii character format if the value is a valid ASCII code (0-127)
		if (registers[rx] <= 127) {
			std::cout << static_cast<char>(registers[rx]);
		}
		break;

	default:
		std::cout << "Unknown opcode: " << op << "\n";
		break;
	}

	PC++;
}


int main(int argc, char* argv[])
{
	// Check if any command-line arguments were passed and parse them
	if (argc > 1) {
		for (int i = 0; i < argc; ++i) {
			// Skip the first argument (program name) when parsing options
			if (i == 0) {
				continue; // Skip the program name
			}
			
			//std::cout << "Argument " << i << ": " << argv[i] << "\n";
			if (std::string(argv[i]) == "--help") {
				std::cout << "Usage: CPU-ASM-EMULATOR [options]\n";
				std::cout << "Options:\n";
				std::cout << "  --help       Show this help message\n";
				std::cout << "  --version    Show version information\n";
				std::cout << "  --verbose    Enable verbose mode\n";
				std::cout << "  --asm [path] Enable assembly mode and asmbl\n";
				std::cout << "  --outbin     Prints the asmbled bin\n";
				std::cout << "  --emulate    Emulates the asmbled bin\n";
				std::cout << "  --nodefaults Do not use default includes (macros)\n";
				std::cout << "\n";
				std::cout << "Example: CPU-ASM-EMULATOR --asm program.asm --verbose --outbin --emulate\n";
				return 0;
			}
			else if (std::string(argv[i]) == "--version") {
				std::cout << "CPU-ASM-EMULATOR version 1.0.0\n";
				return 0;
			}
			else if (std::string(argv[i]) == "--verbose") {
				// Set verbose mode flag to true
				ARG_verboseMode = true;
			} 
			else if (std::string(argv[i]) == "--asm") {
				// Check if the next argument is provided for the assembly file path
				if (i + 1 < argc) {
					if (ARG_verboseMode) {
						// Verbose output :D
						std::string asmFilePath = argv[i + 1];
						std::cout << "Assembly mode enabled. Assembly file: " << asmFilePath << "\n";
					}
					// Set assembly mode flag to true
					ARG_asmMode = true;

					// Set the global variable to store the assembly file path
					::ARG_asmFilePath = argv[i + 1];
					i++; // Skip the next argument since it's the file path
				} else {
					std::cout << "Error: --asm option requires a file path argument.\n";
					return 1; // Exit with an error code
				}
			}
			else if (std::string(argv[i]) == "--outbin") {
				// Set output binary flag to true
				ARG_outbin = true;
			}
			else if (std::string(argv[i]) == "--emulate") {
				// Set emulate flag to true
				ARG_emulate = true;
			}
			else if (std::string(argv[i]) == "--nodefaults") {
				// Set the flag to not use default includes (macros)
				// This will be checked later in the assembly process to determine whether to load default includes or not.
				// If this flag is set, the assembler will skip loading the built-in default includes and only use the provided assembly file content.
				// This allows users to have more control over their assembly code and avoid potential conflicts with default macros.
				// Note: If --nodefaults is set, the assembler will not load any default macros, so users must ensure that their assembly code is self-contained and does not rely on any default macros.
				// This option is useful for advanced users who want to have full control over their assembly code and do not want any predefined macros to interfere with their code.
				// If --nodefaults is not set, the assembler will load the built-in default includes, which may contain useful macros for common operations, but may also cause conflicts if users define their own macros with the same names.
				// Users should choose whether to use --nodefaults based on their specific needs and preferences for their assembly code.
				// For most users, it is recommended to keep the default includes for convenience, unless they have specific reasons to avoid them.
				ARG_nodefaults = true;
			}

			else {
				// Handle unknown arguments
				std::cout << "Unknown argument: " << argv[i] << "\n";
				std::cout << "Use --help to see available options.\n";
			}
		}
	}

	if (ARG_asmMode && ARG_asmFilePath != nullptr) {
		initializeRegisterNames(); // Initialize register names and their corresponding register numbers
		if (ARG_verboseMode) {
			std::cout << "Register names initialized.\n";
		}
		if (ARG_verboseMode) {
			std::cout << "Reading assembly file: " << ARG_asmFilePath << "\n";
		}
		std::ifstream asmFile;
		if (ARG_verboseMode) {
			std::cout << "Opening assembly file: " << ARG_asmFilePath << "\n";
		}
		asmFile.open(ARG_asmFilePath);
		if (asmFile.is_open()) {
			std::string line;
			while (std::getline(asmFile, line)) {
				asmFileContent += line + "\n"; // Append each line to the content variable
			}
			asmFile.close();
			if (ARG_verboseMode) {
				std::cout << "Assembly file read successfully.\n";
				std::cout << "Assembly file content:\n" << asmFileContent << "\n";
			}
		}
		else
		{
			std::cout << "Error: Could not open assembly file: " << ARG_asmFilePath << "\n";
			return 1; // Exit with an error code if the assembly file cannot be opened
		}

		if (!ARG_nodefaults) {
			if (!loadDefaultIncludes()) {
				std::cout << "Error: Failed to load default includes.\n";
				return 1;
			}
		}
		else if (ARG_verboseMode) {
			std::cout << "Skipping loading default includes as --nodefaults is set.\n";
		}

		// Call the function to assemble the assembly code into binary instructions
		if (!assemble()) {
			std::cout << "Error: Assembly failed.\n";
			return 1; // Exit with an error code if assembly fails
		}
	}
	if (ARG_emulate) {
		program = outputBinary;
		PC = 0;
		registers.fill(0); // Initialize all registers to zero before emulation

		while (PC < program.size()) {
			execute(program[PC]);
		}
		// After emulation, print the final state of the registers
		std::cout << "Final register state after emulation:\n";
		for (int i = 0; i < REGISTER_COUNT; ++i) {
			std::cout << "R" << i << ": " << std::bitset<16>(registers[i]) << ": " << registers[i] << "\n";
		}
	}
	else
	{
		return 0; // Exit if assembly mode is not enabled or if the assembly file path is not provided
	}
}
