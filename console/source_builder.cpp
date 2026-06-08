#include "source_builder.h"

#include "../CPU-ASM-EMULATOR/assembler.h"

#include <iostream>
#include <sstream>

namespace console {

SourceBuildResult buildAssemblySource(const std::string& source, bool useDefaultIncludes) {
	SourceBuildResult result;
	std::ostringstream diagnostics;
	auto* oldBuffer = std::cout.rdbuf(diagnostics.rdbuf());

	ARG_verboseMode = false;
	ARG_outbin = false;
	ARG_nodefaults = !useDefaultIncludes;
	initializeRegisterNames();
	asmFileContent = source;
	if (useDefaultIncludes) loadDefaultIncludes();
	result.success = assemble();
	if (result.success) {
		result.rom = buildRomImage();
		result.entryPoint = resetVectorEnabled ? resetVectorAddress : entryPoint;
		diagnostics << "Build succeeded: " << result.rom.size() << " ROM bytes.\n";
	}

	std::cout.rdbuf(oldBuffer);
	result.diagnostics = diagnostics.str();
	return result;
}

} // namespace console
