#include "assembler.h"
#include "cpu.h"

// Global variables to track modes and file paths

bool ARG_verboseMode = false; // Global variable to track verbose mode TODO: make this false by default, but for testing purposes, it's set to true for now
bool ARG_asmMode = false; // Global variable to track assembly mode (so if we need to asmbl)
const char* ARG_asmFilePath = nullptr; // Non-owning pointer to the assembly file path argument
bool ARG_outbin = false; // Global variable to track if the user wants to output the assembled binary
bool ARG_emulate = false; // Global variable to track if the user wants to emulate the assembled binary
bool ARG_nodefaults = false; // Global variable to track if the user wants to skip loading default includes (macros)

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
		if (ARG_verboseMode) {
			std::cout << "Emulation mode enabled. Starting emulation of the assembled binary...\n";
		}

		if (!loadInstructionRom()) {
			return 1;
		}

		PC = 0;
		CPU_halted = false;
		registers.fill(0); // Initialize all registers to zero before emulation
		std::fill(std::begin(memory), std::end(memory), 0); // Initialize all memory to zero before emulation
		SP = 0xFFFE; // Initialize stack pointer to the end of memory (growing downwards)
		
		if (ARG_verboseMode) {
			std::cout << "SP: " << std::bitset<16>(SP) << ": " << SP << "\n";
			std::cout << "Initial register state before emulation:\n";
			for (int i = 0; i < REGISTER_COUNT; ++i) {
				std::cout << "R" << i << ": " << std::bitset<16>(registers[i]) << ": " << registers[i] << "\n";
			}
		}

		while (!CPU_halted && static_cast<size_t>(PC) + INSTRUCTION_SIZE_BYTES <= instructionRomSize) {
			execute(fetchInstruction(PC));
		}

		if (ARG_verboseMode) {
			// After emulation, print the final state of the registers
			std::cout << "Final register state after emulation:\n";
			for (int i = 0; i < REGISTER_COUNT; ++i) {
				std::cout << "R" << i << ": " << std::bitset<16>(registers[i]) << ": " << registers[i] << "\n";
			}
		}
	}
	else
	{
		return 0; // Exit if assembly mode is not enabled or if the assembly file path is not provided
	}
}
