#include <iostream>
#include <map>
#include <string>

// CPU-ASM-EMULATOR CONFIG
const int16_t REGISTER_COUNT = 32; // Number of registers in the CPU emulator (only 16 bits max)
const int REGISTER_SIZE = 16; // Size of each register in bits


// Global variables to track modes and file paths

bool ARG_verboseMode = false; // Global variable to track verbose mode
bool ARG_asmMode = false; // Global variable to track assembly mode (so if we need to asmbl)
char* ARG_asmFilePath = nullptr; // Global variable to store the assembly file path
bool ARG_outbin = false; // Global variable to track if the user wants to output the assembled binary


// Assembly variables

std::map <std::string, int16_t > labels = {}; // Map to store labels and their corresponding line numbers (Program counter position)
std::map <std::string, int16_t > registerNames = {}; // Map to store register names and their corresponding register numbers (e.g., "R0" -> 0000000000000000, "R1" -> 0000000000000001, ..., "R31" -> ...)

// Function to initialize register names and their corresponding register numbers
void initializeRegisterNames() {
	for (int i = 0; i < REGISTER_COUNT; ++i) {
		std::string regName = "R" + std::to_string(i);
		registerNames[regName] = i; // Map register name to its corresponding register number
	}
}


// CPU emulator variables

char* program[]; // Array to store the assembly program instructions (bits)
int PC = 0; // Program Counter
std::map <std::string, int16_t> registers; // Map to store register values (e.g., "R0", "R1", ..., "R31")

// Function to initialize registers
void initializeRegisters() {
	for (int i = 0; i < REGISTER_COUNT; ++i) {
		std::string regName = "R" + std::to_string(i);
		registers[regName] = 0; // Initialize all registers to 0
	}
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
				std::cout << "\n";
				std::cout << "Example: CPU-ASM-EMULATOR --asm program.asm --verbose --outbin\n";
				return 0;
			}
			else if (std::string(argv[i]) == "--version") {
				std::cout << "CPU-ASM-EMULATOR version 1.0.0\n";
				return 0;
			}
			else if (std::string(argv[i]) == "--verbose") {
				std::cout << "Verbose mode enabled.\n";
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

	}
	else
	{
		return 0; // Exit if assembly mode is not enabled or if the assembly file path is not provided
	}
}
