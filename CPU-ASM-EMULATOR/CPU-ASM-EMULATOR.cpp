#include <iostream>

bool verboseMode = false; // Global variable to track verbose mode
bool asmMode = false; // Global variable to track assembly mode (so if we need to asmbl)
char* asmFilePath = nullptr; // Global variable to store the assembly file path

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
                return 0;
            }
            else if (std::string(argv[i]) == "--version") {
                std::cout << "CPU-ASM-EMULATOR version 1.0.0\n";
                return 0;
            }
            else if (std::string(argv[i]) == "--verbose") {
                std::cout << "Verbose mode enabled.\n";
                // Set verbose mode flag to true
                verboseMode = true;
            } 
            else if (std::string(argv[i]) == "--asm") {
				// Check if the next argument is provided for the assembly file path
                if (i + 1 < argc) {
                    if (verboseMode) {
						// Verbose output :D
                        std::string asmFilePath = argv[i + 1];
                        std::cout << "Assembly mode enabled. Assembly file: " << asmFilePath << "\n";
                    }
                    // Set assembly mode flag to true
                    asmMode = true;

					// Set the global variable to store the assembly file path
					::asmFilePath = argv[i + 1];
					i++; // Skip the next argument since it's the file path
                } else {
                    std::cout << "Error: --asm option requires a file path argument.\n";
                    return 1; // Exit with an error code
				}
            }

            else {
				// Handle unknown arguments
                std::cout << "Unknown argument: " << argv[i] << "\n";
				std::cout << "Use --help to see available options.\n";
            }
        }
    }
   



    std::cout << "Hello World!\n";
}
