#include "project.h"

#include <fstream>
#include <regex>
#include <sstream>

namespace console {
namespace {

std::string escapeJson(const std::string& value) {
	std::string result;
	for (char c : value) {
		switch (c) {
		case '\\': result += "\\\\"; break;
		case '"': result += "\\\""; break;
		case '\n': result += "\\n"; break;
		default: result += c; break;
		}
	}
	return result;
}

bool stringValue(const std::string& json, const std::string& key, std::string& value) {
	const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
	std::smatch match;
	if (!std::regex_search(json, match, pattern)) return false;
	value = match[1].str();
	return true;
}

bool integerValue(const std::string& json, const std::string& key, uint64_t& value) {
	const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
	std::smatch match;
	if (!std::regex_search(json, match, pattern)) return false;
	value = std::stoull(match[1].str());
	return true;
}

} // namespace

bool loadProject(const std::string& path, ConsoleProject& project, std::string& error) {
	std::ifstream input(path);
	if (!input) {
		error = "Could not open project: " + path;
		return false;
	}
	std::ostringstream buffer;
	buffer << input.rdbuf();
	const std::string json = buffer.str();

	stringValue(json, "name", project.name);
	stringValue(json, "source", project.sourcePath);
	stringValue(json, "storage", project.storagePath);
	stringValue(json, "asset", project.assetPath);
	std::string profileName;
	if (stringValue(json, "profile", profileName)) {
		if (profileName == "home") project.profile = ProfileId::Home;
		else if (profileName == "studio") project.profile = ProfileId::Studio;
		else project.profile = ProfileId::Pocket;
	}

	uint64_t value = 0;
	if (integerValue(json, "clockHz", value)) project.studio.clockHz = value;
	if (integerValue(json, "displayWidth", value)) project.studio.displayWidth = static_cast<uint32_t>(value);
	if (integerValue(json, "displayHeight", value)) project.studio.displayHeight = static_cast<uint32_t>(value);
	if (integerValue(json, "ramBytes", value)) project.studio.ramBytes = static_cast<size_t>(value);
	if (integerValue(json, "romBytes", value)) project.studio.romBytes = static_cast<size_t>(value);
	if (integerValue(json, "vramBytes", value)) project.studio.vramBytes = static_cast<size_t>(value);
	if (integerValue(json, "storageBytes", value)) project.studio.storageBytes = static_cast<size_t>(value);
	if (integerValue(json, "maxSprites", value)) project.studio.maxSprites = static_cast<uint32_t>(value);
	if (integerValue(json, "spritesPerScanline", value)) project.studio.spritesPerScanline = static_cast<uint32_t>(value);
	if (integerValue(json, "paletteColors", value)) project.studio.paletteColors = static_cast<uint32_t>(value);
	if (integerValue(json, "audioChannels", value)) project.studio.audioChannels = static_cast<uint32_t>(value);
	const std::array<const char*, ConsoleButtonCount> buttonNames = { "Up", "Down", "Left", "Right", "A", "B", "Start", "Select" };
	for (size_t i = 0; i < buttonNames.size(); ++i) {
		if (integerValue(json, std::string("key") + buttonNames[i], value)) project.input.keyboard[i] = static_cast<int32_t>(value);
		if (integerValue(json, std::string("pad") + buttonNames[i], value)) project.input.gamepad[i] = static_cast<int32_t>(value);
	}

	if (!validateProfile(project.profile == ProfileId::Studio ? project.studio : profileFor(project.profile), error)) {
		return false;
	}
	error.clear();
	return true;
}

bool saveProject(const std::string& path, const ConsoleProject& project, std::string& error) {
	std::ofstream output(path);
	if (!output) {
		error = "Could not write project: " + path;
		return false;
	}
	const char* profileName = project.profile == ProfileId::Home ? "home" :
		project.profile == ProfileId::Studio ? "studio" : "pocket";
	output << "{\n"
		<< "  \"name\": \"" << escapeJson(project.name) << "\",\n"
		<< "  \"source\": \"" << escapeJson(project.sourcePath) << "\",\n"
		<< "  \"storage\": \"" << escapeJson(project.storagePath) << "\",\n"
		<< "  \"asset\": \"" << escapeJson(project.assetPath) << "\",\n"
		<< "  \"profile\": \"" << profileName << "\",\n"
		<< "  \"studio\": {\n"
		<< "    \"clockHz\": " << project.studio.clockHz << ",\n"
		<< "    \"displayWidth\": " << project.studio.displayWidth << ",\n"
		<< "    \"displayHeight\": " << project.studio.displayHeight << ",\n"
		<< "    \"ramBytes\": " << project.studio.ramBytes << ",\n"
		<< "    \"romBytes\": " << project.studio.romBytes << ",\n"
		<< "    \"vramBytes\": " << project.studio.vramBytes << ",\n"
		<< "    \"storageBytes\": " << project.studio.storageBytes << ",\n"
		<< "    \"maxSprites\": " << project.studio.maxSprites << ",\n"
		<< "    \"spritesPerScanline\": " << project.studio.spritesPerScanline << ",\n"
		<< "    \"paletteColors\": " << project.studio.paletteColors << ",\n"
		<< "    \"audioChannels\": " << project.studio.audioChannels << "\n"
		<< "  },\n"
		<< "  \"input\": {\n"
		<< "    \"keyUp\": " << project.input.keyboard[0] << ", \"keyDown\": " << project.input.keyboard[1] << ",\n"
		<< "    \"keyLeft\": " << project.input.keyboard[2] << ", \"keyRight\": " << project.input.keyboard[3] << ",\n"
		<< "    \"keyA\": " << project.input.keyboard[4] << ", \"keyB\": " << project.input.keyboard[5] << ",\n"
		<< "    \"keyStart\": " << project.input.keyboard[6] << ", \"keySelect\": " << project.input.keyboard[7] << ",\n"
		<< "    \"padUp\": " << project.input.gamepad[0] << ", \"padDown\": " << project.input.gamepad[1] << ",\n"
		<< "    \"padLeft\": " << project.input.gamepad[2] << ", \"padRight\": " << project.input.gamepad[3] << ",\n"
		<< "    \"padA\": " << project.input.gamepad[4] << ", \"padB\": " << project.input.gamepad[5] << ",\n"
		<< "    \"padStart\": " << project.input.gamepad[6] << ", \"padSelect\": " << project.input.gamepad[7] << "\n"
		<< "  }\n"
		<< "}\n";
	error.clear();
	return true;
}

} // namespace console
