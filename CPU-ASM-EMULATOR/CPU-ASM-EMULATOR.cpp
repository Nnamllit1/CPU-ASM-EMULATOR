#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <bitset>

// CPU-ASM-EMULATOR CONFIG
const int16_t REGISTER_COUNT = 32; // Number of registers in the CPU emulator (only 16 bits max)
const int REGISTER_SIZE = 16; // Size of each register in bits


// Global variables to track modes and file paths

bool ARG_verboseMode = true; // Global variable to track verbose mode TODO: make this false by default, but for testing purposes, it's set to true for now
bool ARG_asmMode = false; // Global variable to track assembly mode (so if we need to asmbl)
char* ARG_asmFilePath = nullptr; // Global variable to store the assembly file path
bool ARG_outbin = false; // Global variable to track if the user wants to output the assembled binary


// Assembly variables

std::map <std::string, int16_t> labels = {}; // Map to store labels and their corresponding line numbers (Program counter position)
std::vector<uint64_t> outputBinary; // Vector to store the output binary instructions (bits)
std::map <std::string, int16_t> registerNames = {}; // Map to store register names and their corresponding register numbers (e.g., "R0" -> 0000000000000000, "R1" -> 0000000000000001, ..., "R31" -> ...)
std::string asmFileContent = ""; // Variable to store the content of the assembly file as a string


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

struct InstructionDef { // Struct to represent an instruction definition, including its mnemonic, opcode, and operand kinds
	std::string mnemonic;
	uint16_t opcode;
	std::vector<OperandKind> operands;
};

const std::map<std::string, InstructionDef> instructionSet = { // Map to store instruction definitions, including their mnemonics, opcodes, and operand kinds
	{"movi", {"movi", 0x0000, {OperandKind::Register, OperandKind::Immediate}}},
	{"movr", {"movr", 0x0001, {OperandKind::Register, OperandKind::Register}}},
	{"mov",  {"mov",  0x0002, {OperandKind::Register, OperandKind::Register}}},
	{"add",  {"add",  0x0003, {OperandKind::Register, OperandKind::Register}}},
	{"sub",  {"sub",  0x0004, {OperandKind::Register, OperandKind::Register}}},
	{"srl",  {"srl",  0x0005, {OperandKind::Register, OperandKind::Register}}},
	{"srr",  {"srr",  0x0006, {OperandKind::Register, OperandKind::Register}}},
	{"jmp",  {"jmp",  0x0007, {OperandKind::Label}}}
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
	// Find instruction definition (opcode and operand kinds) for the mnemonic.
	auto it = instructionSet.find(ins.mnemonic);
	if (it == instructionSet.end()) {
		throw std::runtime_error("Unknown instruction: " + ins.mnemonic);
	}

	const InstructionDef& def = it->second;

	// Validate operand count: instruction definition declares expected operand kinds.
	if (ins.operands.size() != def.operands.size()) {
		throw std::runtime_error("Wrong operand count for " + ins.mnemonic);
	}

	// fields: [opcode, op1, op2, op3] each 16 bits -> combined into 64-bit word
	uint16_t fields[4] = { 0, 0, 0, 0 };
	fields[0] = def.opcode;

	// Encode each operand into its 16-bit slot.
	for (size_t i = 0; i < def.operands.size(); ++i) {
		fields[i + 1] = encodeOperand(ins.operands[i], def.operands[i]);
	}

	// Place fields into a 64-bit word: opcode in highest 16 bits, then operand slots.
	uint64_t encoded = 0;
	encoded |= static_cast<uint64_t>(fields[0]) << 48;
	encoded |= static_cast<uint64_t>(fields[1]) << 32;
	encoded |= static_cast<uint64_t>(fields[2]) << 16;
	encoded |= static_cast<uint64_t>(fields[3]);
	return encoded;
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

// Function to parse a line of assembly code into a ParsedInstruction struct, including the mnemonic and operands
ParsedInstruction parseLine(const std::string& rawLine) {
	// Copy input and normalize comma separators to spaces so >> extraction works uniformly.
	std::string line = rawLine;
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

// Assembler function to convert assembly code into binary instructions (64-bit)
bool assemble() {
	if (asmFileContent == "") return false; // Return false if the assembly file content is empty

	// First pass: Process labels and store their corresponding line numbers (Program counter position) in the labels map
	asmFileContent = trim(asmFileContent); // Trim whitespace from the assembly file content
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
				std::cout << "Instruction " << i << ": " << std::bitset<64>(outputBinary[i]) << "\n";
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
std::array<uint16_t, REGISTER_COUNT>; // Array to represent the registers in the CPU emulator (16-bit registers)


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
				std::cout << "\n";
				std::cout << "Example: CPU-ASM-EMULATOR --asm program.asm --verbose --outbin\n";
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

		// Call the function to assemble the assembly code into binary instructions
		assemble();
	}
	else
	{
		return 0; // Exit if assembly mode is not enabled or if the assembly file path is not provided
	}
}
