#include "../console/asset_import.h"
#include "../console/console_machine.h"
#include "../console/disassembler.h"
#include "../console/playback.h"
#include "../console/project.h"
#include "../console/source_builder.h"

#include <SDL3/SDL.h>
#include <TextEditor.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <dejavu.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr size_t PathCapacity = 512;
constexpr std::array<const char*, console::ConsoleButtonCount> ButtonNames = {
	"Up", "Down", "Left", "Right", "A", "B", "Start", "Select"
};

struct DesktopPreferences {
	float executionSpeed = 1.0f;
	bool unlimited = false;
};

const char* defaultSource = R"asm(; CPU ASM console: RGB332 color bars.
; Click the editor gutter to add a source breakpoint.

start:
    movi r0, 0xE0
    movi r1, 0xC000
    movi r2, 1
    movi r3, 0xC100

draw_red:
    stb r1, r0
    add r1, r1, r2
    jlt r1, r3, draw_red

    movi r0, 0x1C
    movi r3, 0xC200
draw_green:
    stb r1, r0
    add r1, r1, r2
    jlt r1, r3, draw_green

    movi r0, 0x03
    movi r3, 0xC300
draw_blue:
    stb r1, r0
    add r1, r1, r2
    jlt r1, r3, draw_blue

    %present r4
    hlt
)asm";

bool readTextFile(const std::string& path, std::string& text) {
	std::ifstream input(path, std::ios::binary);
	if (!input) return false;
	std::ostringstream buffer;
	buffer << input.rdbuf();
	text = buffer.str();
	return true;
}

bool writeTextFile(const std::string& path, const std::string& text) {
	std::ofstream output(path, std::ios::binary);
	if (!output) return false;
	output << text;
	return output.good();
}

std::string desktopPreferencesPath() {
	char* directory = SDL_GetPrefPath("Nnamllit1", "CPU-ASM-CONSOLE");
	if (!directory) return {};
	const std::string path = std::string(directory) + "desktop-settings.txt";
	SDL_free(directory);
	return path;
}

DesktopPreferences loadDesktopPreferences(const std::string& path) {
	DesktopPreferences preferences;
	std::ifstream input(path);
	std::string key;
	while (input >> key) {
		if (key == "executionSpeed") input >> preferences.executionSpeed;
		else if (key == "unlimited") input >> preferences.unlimited;
	}
	if (!std::isfinite(preferences.executionSpeed) || preferences.executionSpeed < 0.05f || preferences.executionSpeed > 10.0f) {
		preferences.executionSpeed = 1.0f;
	}
	return preferences;
}

void saveDesktopPreferences(const std::string& path, const DesktopPreferences& preferences) {
	if (path.empty()) return;
	std::ofstream output(path, std::ios::trunc);
	if (!output) return;
	output << "executionSpeed " << preferences.executionSpeed << '\n'
		<< "unlimited " << preferences.unlimited << '\n';
}

const char* stateName(console::MachineState state) {
	switch (state) {
	case console::MachineState::Ready: return "Ready";
	case console::MachineState::Running: return "Running";
	case console::MachineState::Paused: return "Paused";
	case console::MachineState::Halted: return "Halted";
	case console::MachineState::Faulted: return "Faulted";
	}
	return "Unknown";
}

void applyStyle() {
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 5.0f;
	style.ChildRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.TabRounding = 4.0f;
	style.ScrollbarRounding = 4.0f;
	style.GrabRounding = 4.0f;
	style.WindowPadding = ImVec2(10, 9);
	style.FramePadding = ImVec2(8, 5);
	style.ItemSpacing = ImVec2(8, 7);
	style.ItemInnerSpacing = ImVec2(6, 5);
	style.ScrollbarSize = 13.0f;
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.082f, 0.095f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.086f, 0.094f, 0.108f, 1.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.19f, 0.22f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.16f, 0.20f, 0.23f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.29f, 0.33f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.16f, 0.20f, 0.23f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.30f, 0.34f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.27f, 0.39f, 0.43f, 1.0f);
	style.Colors[ImGuiCol_TabSelected] = ImVec4(0.18f, 0.25f, 0.28f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.38f, 0.78f, 0.67f, 1.0f);
}

const TextEditor::Language* assemblyLanguage() {
	static const TextEditor::Language language = [] {
		TextEditor::Language value = *TextEditor::Language::C();
		value.name = "CPU ASM";
		value.caseSensitive = false;
		value.singleLineComment = ";";
		value.commentStart.clear();
		value.commentEnd.clear();
		value.preprocess = '%';
		value.keywords = {
			"mov", "movc", "movi", "add", "sub", "mul", "div", "mod", "and", "or", "xor", "not",
			"shl", "shr", "jmp", "jz", "jnz", "je", "jne", "jlt", "jle", "jgt", "jge", "call", "ret",
			"push", "pop", "hlt", "out", "outn", "outs", "in", "inkey", "ld", "ldi", "st", "sti",
			"ldb", "stb", "ldbi", "stbi", "ldbr", "ldbri", "ldwr", "ldwri", ".byte", ".word", ".ascii",
			".asciiz", ".space", ".align", ".org", ".entry", ".reset", "%macro", "%endmacro"
		};
		for (int i = 0; i < 32; ++i) value.identifiers.insert("r" + std::to_string(i));
		return value;
	}();
	return &language;
}

void configureDefaultDocking(ImGuiID dockspaceId, const ImVec2& size) {
	const ImGuiDockNode* existing = ImGui::DockBuilderGetNode(dockspaceId);
	if (existing && (existing->ChildNodes[0] || existing->ChildNodes[1])) return;
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, size);
	ImGuiID center = dockspaceId;
	ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.29f, nullptr, &center);
	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);
	ImGuiID display = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.38f, nullptr, &center);
	ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.48f, nullptr, &right);
	ImGui::DockBuilderDockWindow("Assembly Editor", center);
	ImGui::DockBuilderDockWindow("Console", display);
	ImGui::DockBuilderDockWindow("Debugger", right);
	ImGui::DockBuilderDockWindow("Inspector", rightBottom);
	ImGui::DockBuilderDockWindow("Output", bottom);
	ImGui::DockBuilderFinish(dockspaceId);
}

bool circularControl(const char* id, const char* label, const ImVec2& size, ImU32 color, bool& held) {
	ImGui::InvisibleButton(id, size);
	const bool activated = ImGui::IsItemActivated();
	held = ImGui::IsItemActive();
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
	const float radius = std::min(size.x, size.y) * 0.46f;
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircleFilled(center, radius, held ? IM_COL32(235, 238, 240, 255) : color, 32);
	draw->AddCircle(center, radius, IM_COL32(10, 13, 16, 210), 32, 2.0f);
	const ImVec2 textSize = ImGui::CalcTextSize(label);
	draw->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
		held ? IM_COL32(25, 29, 32, 255) : IM_COL32(245, 247, 248, 255), label);
	return activated;
}

bool slimControl(const char* id, const char* label, const ImVec2& size, bool& held) {
	ImGui::InvisibleButton(id, size);
	const bool activated = ImGui::IsItemActivated();
	held = ImGui::IsItemActive();
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(min, max, held ? IM_COL32(210, 216, 219, 255) : IM_COL32(64, 72, 78, 255), size.y * 0.5f);
	draw->AddRect(min, max, IM_COL32(12, 15, 18, 220), size.y * 0.5f, 0, 1.5f);
	const ImVec2 textSize = ImGui::CalcTextSize(label);
	draw->AddText(ImVec2((min.x + max.x - textSize.x) * 0.5f, (min.y + max.y - textSize.y) * 0.5f),
		held ? IM_COL32(20, 24, 27, 255) : IM_COL32(225, 229, 232, 255), label);
	return activated;
}

void clearEditorBreakpoints(TextEditor& editor) {
	std::vector<int> lines;
	editor.IterateUserData([&](int line, void*) { lines.push_back(line); });
	for (int line : lines) editor.SetUserData(line, nullptr);
}

std::set<int> editorBreakpointLines(const TextEditor& editor) {
	std::set<int> lines;
	editor.IterateUserData([&](int line, void* data) {
		if (data) lines.insert(line + 1);
	});
	return lines;
}

void renderHexBuffer(const char* id, const std::vector<uint8_t>& bytes, size_t& address) {
	if (bytes.empty()) {
		ImGui::TextDisabled("This region is not available in the current profile.");
		return;
	}
	uint64_t requested = address;
	ImGui::SetNextItemWidth(150.0f);
	if (ImGui::InputScalar("Address", ImGuiDataType_U64, &requested, nullptr, nullptr, "%08llX", ImGuiInputTextFlags_CharsHexadecimal)) {
		address = std::min<size_t>(static_cast<size_t>(requested), bytes.size() - 1);
	}
	ImGui::SameLine();
	const bool scrollToAddress = ImGui::Button("Go");
	ImGui::Separator();
	if (scrollToAddress) ImGui::SetNextWindowScroll(ImVec2(0, static_cast<float>((address / 16) * ImGui::GetTextLineHeightWithSpacing())));
	ImGui::BeginChild(id, ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
	const int rows = static_cast<int>((bytes.size() + 15) / 16);
	ImGuiListClipper clipper;
	clipper.Begin(rows, ImGui::GetTextLineHeightWithSpacing());
	while (clipper.Step()) {
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
			const size_t offset = static_cast<size_t>(row) * 16;
			std::ostringstream line;
			line << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << offset << "  ";
			for (size_t column = 0; column < 16; ++column) {
				if (offset + column < bytes.size()) line << std::setw(2) << static_cast<int>(bytes[offset + column]) << ' ';
				else line << "   ";
			}
			line << " ";
			for (size_t column = 0; column < 16 && offset + column < bytes.size(); ++column) {
				const uint8_t value = bytes[offset + column];
				line << (value >= 32 && value <= 126 ? static_cast<char>(value) : '.');
			}
			if (address >= offset && address < offset + 16) ImGui::TextColored(ImVec4(0.40f, 0.82f, 0.72f, 1), "%s", line.str().c_str());
			else ImGui::TextUnformatted(line.str().c_str());
		}
	}
	ImGui::EndChild();
}

ImU32 rgb332Color(uint8_t value) {
	const int red = ((value >> 5) & 7) * 255 / 7;
	const int green = ((value >> 2) & 7) * 255 / 7;
	const int blue = (value & 3) * 255 / 3;
	return IM_COL32(red, green, blue, 255);
}

void renderVramPixels(const std::vector<uint8_t>& vram, size_t& offset) {
	if (vram.empty()) return;
	uint64_t requested = offset;
	ImGui::SetNextItemWidth(150.0f);
	if (ImGui::InputScalar("RGB332 offset", ImGuiDataType_U64, &requested, nullptr, nullptr, "%08llX", ImGuiInputTextFlags_CharsHexadecimal)) {
		offset = std::min<size_t>(static_cast<size_t>(requested), vram.size() - 1);
	}
	const int columns = 32;
	const int rows = 16;
	const float cell = std::clamp(ImGui::GetContentRegionAvail().x / columns, 4.0f, 16.0f);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList* draw = ImGui::GetWindowDrawList();
	for (int y = 0; y < rows; ++y) {
		for (int x = 0; x < columns; ++x) {
			const size_t index = offset + static_cast<size_t>(y * columns + x);
			if (index >= vram.size()) break;
			const ImVec2 a(origin.x + x * cell, origin.y + y * cell);
			draw->AddRectFilled(a, ImVec2(a.x + cell, a.y + cell), rgb332Color(vram[index]));
		}
	}
	ImGui::Dummy(ImVec2(columns * cell, rows * cell));
}

bool bindingMatches(const std::array<int32_t, console::ConsoleButtonCount>& bindings, int value, size_t& index) {
	for (size_t i = 0; i < bindings.size(); ++i) {
		if (bindings[i] == value) {
			index = i;
			return true;
		}
	}
	return false;
}

} // namespace

int main(int, char**) {
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
		std::cerr << "SDL initialization failed: " << SDL_GetError() << "\n";
		return 1;
	}
	const std::string preferencesPath = desktopPreferencesPath();
	DesktopPreferences preferences = loadDesktopPreferences(preferencesPath);

	SDL_Window* window = SDL_CreateWindow("CPU ASM Console", 1500, 920, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
	if (!window || !renderer) {
		std::cerr << "SDL window/renderer creation failed: " << SDL_GetError() << "\n";
		if (renderer) SDL_DestroyRenderer(renderer);
		if (window) SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}
	SDL_SetRenderVSync(renderer, 1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiContext* mainContext = ImGui::GetCurrentContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
	ImFontConfig fontConfig;
	fontConfig.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryCompressedTTF(const_cast<unsigned int*>(dejavu), dejavuSize, 17.0f, &fontConfig);
	applyStyle();
	ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);

	SDL_AudioSpec audioSpec{};
	audioSpec.format = SDL_AUDIO_F32;
	audioSpec.channels = 1;
	audioSpec.freq = 48'000;
	SDL_AudioStream* audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, nullptr, nullptr);
	if (audioStream) SDL_ResumeAudioStreamDevice(audioStream);

	SDL_Gamepad* gamepad = nullptr;
	int gamepadCount = 0;
	if (SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepadCount)) {
		if (gamepadCount > 0) gamepad = SDL_OpenGamepad(gamepads[0]);
		SDL_free(gamepads);
	}

	console::ConsoleProject project;
	console::ConsoleMachine machine(console::pocketProfile());
	TextEditor editor;
	editor.SetLanguage(assemblyLanguage());
	editor.SetText(defaultSource);
	editor.SetLineSpacing(1.12f);
	editor.SetShowScrollbarMiniMapEnabled(true);
	editor.SetAutoIndentEnabled(true);

	std::array<char, PathCapacity> sourcePath{};
	std::array<char, PathCapacity> projectPath{};
	std::array<char, PathCapacity> assetPath{};
	std::array<char, PathCapacity> storagePath{};
	std::strncpy(sourcePath.data(), "examples/console/snake.asm", sourcePath.size() - 1);
	std::strncpy(projectPath.data(), "examples/console/snake.console.json", projectPath.size() - 1);
	std::strncpy(assetPath.data(), "examples/console/checker.ppm", assetPath.size() - 1);
	std::strncpy(storagePath.data(), "console-storage.sav", storagePath.size() - 1);
	std::string initialSource;
	if (!readTextFile(sourcePath.data(), initialSource)) {
		readTextFile(std::string(CPU_ASM_SOURCE_DIR) + "/examples/console/snake.asm", initialSource);
	}
	if (!initialSource.empty()) editor.SetText(initialSource);

	std::string diagnostics = "Run assembles the current source, loads a fresh ROM, and starts it.\n";
	console::SourceBuildResult currentBuild;
	bool sourceDirty = true;
	bool validBuild = false;
	bool centerExecutionLine = false;
	bool settingsOpen = false;
	bool resetWorkspace = false;
	float uiScale = 0.88f;
	float editorLineSpacing = 1.08f;
	float executionSpeed = preferences.executionSpeed;
	bool unlimitedSpeed = preferences.unlimited;
	SDL_Window* settingsWindow = nullptr;
	SDL_Renderer* settingsRenderer = nullptr;
	ImGuiContext* settingsContext = nullptr;
	editor.SetChangeCallback([&] { sourceDirty = true; }, 50);

	std::array<bool, console::ConsoleButtonCount> keyboardHeld{};
	std::array<bool, console::ConsoleButtonCount> gamepadHeld{};
	std::array<bool, console::ConsoleButtonCount> screenHeld{};
	int captureKeyboard = -1;
	int captureGamepad = -1;
	int selectedProfile = 0;
	size_t ramAddress = 0;
	size_t vramAddress = 0;
	size_t romAddress = 0;
	size_t storageAddress = 0;
	size_t vramPixelOffset = 0;

	SDL_Texture* displayTexture = nullptr;
	uint32_t textureWidth = 0;
	uint32_t textureHeight = 0;
	bool running = true;
	uint64_t lastTick = SDL_GetTicksNS();
	double cycleAccumulator = 0.0;
	auto applySpeedPreference = [&]() {
		executionSpeed = std::clamp(executionSpeed, 0.05f, 10.0f);
		cycleAccumulator = 0.0;
		machine.drainAudioSamples();
		if (audioStream) SDL_ClearAudioStream(audioStream);
		preferences.executionSpeed = executionSpeed;
		preferences.unlimited = unlimitedSpeed;
		saveDesktopPreferences(preferencesPath, preferences);
	};

	auto synchronizeBreakpoints = [&]() {
		machine.clearBreakpoints();
		if (!validBuild) return;
		for (int line : editorBreakpointLines(editor)) {
			const auto address = currentBuild.sourceLineToAddress.find(line);
			if (address != currentBuild.sourceLineToAddress.end()) machine.addBreakpoint(address->second);
		}
	};

	auto configureMachine = [&]() {
		project.profile = static_cast<console::ProfileId>(selectedProfile);
		const console::HardwareProfile profile = project.profile == console::ProfileId::Studio ? project.studio : console::profileFor(project.profile);
		std::string error;
		if (!machine.configure(profile, error)) diagnostics = "Profile error: " + error + "\n";
		validBuild = false;
		sourceDirty = true;
		cycleAccumulator = 0.0;
	};

	auto buildAndLoad = [&](bool startRunning) {
		machine.pause();
		console::SourceBuildResult build = console::buildAssemblySource(editor.GetText());
		diagnostics = build.diagnostics;
		if (!build.success) {
			validBuild = false;
			return false;
		}
		std::string error;
		if (!machine.loadRom(build.rom, build.entryPoint, error)) {
			diagnostics += error + "\n";
			validBuild = false;
			return false;
		}
		currentBuild = std::move(build);
		validBuild = true;
		sourceDirty = false;
		synchronizeBreakpoints();
		if (startRunning) machine.run();
		centerExecutionLine = true;
		return true;
	};

	auto createSettingsWindow = [&]() {
		if (settingsWindow) return true;
		settingsWindow = SDL_CreateWindow("CPU ASM Settings", 720, 780,
			SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
		settingsRenderer = settingsWindow ? SDL_CreateRenderer(settingsWindow, nullptr) : nullptr;
		if (!settingsWindow || !settingsRenderer) {
			diagnostics = std::string("Could not open settings window: ") + SDL_GetError() + "\n";
			if (settingsRenderer) SDL_DestroyRenderer(settingsRenderer);
			if (settingsWindow) SDL_DestroyWindow(settingsWindow);
			settingsRenderer = nullptr;
			settingsWindow = nullptr;
			settingsOpen = false;
			return false;
		}
		SDL_SetRenderVSync(settingsRenderer, 1);
		settingsContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(settingsContext);
		ImGuiIO& settingsIo = ImGui::GetIO();
		settingsIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImFontConfig settingsFontConfig;
		settingsFontConfig.FontDataOwnedByAtlas = false;
		settingsIo.Fonts->AddFontFromMemoryCompressedTTF(const_cast<unsigned int*>(dejavu), dejavuSize, 16.0f, &settingsFontConfig);
		applyStyle();
		ImGui_ImplSDL3_InitForSDLRenderer(settingsWindow, settingsRenderer);
		ImGui_ImplSDLRenderer3_Init(settingsRenderer);
		ImGui::SetCurrentContext(mainContext);
		return true;
	};

	auto destroySettingsWindow = [&]() {
		if (!settingsContext) return;
		ImGui::SetCurrentContext(settingsContext);
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext(settingsContext);
		settingsContext = nullptr;
		SDL_DestroyRenderer(settingsRenderer);
		SDL_DestroyWindow(settingsWindow);
		settingsRenderer = nullptr;
		settingsWindow = nullptr;
		ImGui::SetCurrentContext(mainContext);
	};

	auto renderSettingsContent = [&]() {
		if (ImGui::BeginTabBar("settings-tabs")) {
			if (ImGui::BeginTabItem("Project")) {
				const char* profiles[] = { "Pocket Color", "Home 16", "Custom Hardware" };
				const int previousProfile = selectedProfile;
				if (ImGui::Combo("Hardware profile", &selectedProfile, profiles, 3)) {
					if (selectedProfile == static_cast<int>(console::ProfileId::Studio) &&
						previousProfile != static_cast<int>(console::ProfileId::Studio)) {
						project.studio = console::customProfileFrom(static_cast<console::ProfileId>(previousProfile));
					}
					configureMachine();
				}
				if (selectedProfile == static_cast<int>(console::ProfileId::Studio)) {
					ImGui::TextWrapped("Custom Hardware lets you start from a known console and override only the limits you want to change.");
					ImGui::SeparatorText("Custom hardware");
					if (ImGui::BeginCombo("Start from", "Choose reference preset...")) {
						for (int reference = static_cast<int>(console::ProfileId::Pocket);
							reference <= static_cast<int>(console::ProfileId::Studio); ++reference) {
							const auto referenceId = static_cast<console::ProfileId>(reference);
							const auto referenceProfile = console::profileFor(referenceId);
							if (ImGui::Selectable(referenceProfile.name.c_str())) {
								project.studio = console::customProfileFrom(referenceId);
								configureMachine();
							}
						}
						ImGui::EndCombo();
					}
					ImGui::TextDisabled("Copies the display, memory, graphics, and audio limits. You can then change any field.");
					const uint64_t clockStep = 1'000;
					const uint64_t clockFastStep = 1'000'000;
					if (ImGui::InputScalar("Clock Hz", ImGuiDataType_U64, &project.studio.clockHz,
						&clockStep, &clockFastStep, "%llu")) {
						project.studio.clockHz = std::clamp<uint64_t>(project.studio.clockHz, 1, 1'000'000'000ULL);
					}
					int width = static_cast<int>(project.studio.displayWidth);
					int height = static_cast<int>(project.studio.displayHeight);
					int refreshRate = static_cast<int>(project.studio.framesPerSecond);
					if (ImGui::SliderInt("Display width", &width, 64, 1920)) project.studio.displayWidth = static_cast<uint32_t>(width);
					if (ImGui::SliderInt("Display height", &height, 64, 1080)) project.studio.displayHeight = static_cast<uint32_t>(height);
					if (ImGui::SliderInt("Refresh Hz", &refreshRate, 1, 240)) project.studio.framesPerSecond = static_cast<uint32_t>(refreshRate);
					auto memoryInput = [](const char* label, size_t& bytes, uint64_t minimumKiB, uint64_t maximumKiB) {
						uint64_t kibibytes = static_cast<uint64_t>(bytes / 1024);
						const uint64_t step = 64;
						const uint64_t fastStep = 1024;
						if (ImGui::InputScalar(label, ImGuiDataType_U64, &kibibytes, &step, &fastStep, "%llu")) {
							kibibytes = std::clamp(kibibytes, minimumKiB, maximumKiB);
							bytes = static_cast<size_t>(kibibytes * 1024);
						}
					};
					memoryInput("RAM KiB", project.studio.ramBytes, 64, 256 * 1024);
					memoryInput("ROM KiB", project.studio.romBytes, 64, 512 * 1024);
					memoryInput("VRAM KiB", project.studio.vramBytes, 1, 256 * 1024);
					memoryInput("Storage KiB", project.studio.storageBytes, 0, 256 * 1024);
					int sprites = static_cast<int>(project.studio.maxSprites);
					int scanlineSprites = static_cast<int>(project.studio.spritesPerScanline);
					int paletteColors = static_cast<int>(project.studio.paletteColors);
					int audioChannels = static_cast<int>(project.studio.audioChannels);
					ImGui::SliderInt("Sprites", &sprites, 0, 4096);
					ImGui::SliderInt("Sprites per scanline", &scanlineSprites, 0, 1024);
					ImGui::SliderInt("Palette colors", &paletteColors, 2, 256);
					ImGui::SliderInt("Audio channels", &audioChannels, 0, 64);
					project.studio.maxSprites = static_cast<uint32_t>(sprites);
					project.studio.spritesPerScanline = static_cast<uint32_t>(scanlineSprites);
					project.studio.paletteColors = static_cast<uint32_t>(paletteColors);
					project.studio.audioChannels = static_cast<uint32_t>(audioChannels);
					if (ImGui::Button("Apply custom hardware")) configureMachine();
				}
				ImGui::SeparatorText("Project file");
				ImGui::InputText("Project", projectPath.data(), projectPath.size());
				if (ImGui::Button("Load Project")) {
					std::string error;
					if (console::loadProject(projectPath.data(), project, error)) {
						selectedProfile = static_cast<int>(project.profile);
						std::snprintf(sourcePath.data(), sourcePath.size(), "%s", project.sourcePath.c_str());
						std::snprintf(storagePath.data(), storagePath.size(), "%s", project.storagePath.c_str());
						std::snprintf(assetPath.data(), assetPath.size(), "%s", project.assetPath.c_str());
						configureMachine();
						std::string text;
						if (readTextFile(sourcePath.data(), text)) editor.SetText(text);
						clearEditorBreakpoints(editor);
						diagnostics = "Project loaded.\n";
					} else diagnostics = error + "\n";
				}
				ImGui::SameLine();
				if (ImGui::Button("Save Project")) {
					project.sourcePath = sourcePath.data(); project.storagePath = storagePath.data(); project.assetPath = assetPath.data();
					std::string error;
					diagnostics = console::saveProject(projectPath.data(), project, error) ? "Project saved.\n" : error + "\n";
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Assets")) {
				ImGui::InputText("Storage file", storagePath.data(), storagePath.size());
				if (ImGui::Button("Load Storage")) { std::string error; diagnostics = machine.loadStorageFile(storagePath.data(), error) ? "Storage loaded.\n" : error + "\n"; }
				ImGui::SameLine();
				if (ImGui::Button("Save Storage")) { std::string error; diagnostics = machine.saveStorageFile(storagePath.data(), error) ? "Storage saved.\n" : error + "\n"; }
				ImGui::InputText("PPM image", assetPath.data(), assetPath.size());
				if (ImGui::Button("Import to VRAM")) {
					console::ImportedImage image; std::string error;
					if (console::importPpmRgb332(assetPath.data(), image, error) && machine.loadVram(image.rgb332, 0, error)) diagnostics = "Imported RGB332 image.\n";
					else diagnostics = error + "\n";
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Controls")) {
				ImGui::TextDisabled("Physical controller: %s", gamepad ? SDL_GetGamepadName(gamepad) : "not connected");
				if (ImGui::BeginTable("bindings", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
					ImGui::TableSetupColumn("Action"); ImGui::TableSetupColumn("Keyboard"); ImGui::TableSetupColumn("Gamepad"); ImGui::TableHeadersRow();
					for (size_t i = 0; i < console::ConsoleButtonCount; ++i) {
						ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(ButtonNames[i]);
						ImGui::TableNextColumn(); ImGui::PushID(static_cast<int>(i));
						const std::string keyLabel = captureKeyboard == static_cast<int>(i) ? "Press key..." : SDL_GetKeyName(project.input.keyboard[i]);
						if (ImGui::Button(keyLabel.empty() ? "Bind key" : keyLabel.c_str(), ImVec2(-1, 0))) captureKeyboard = static_cast<int>(i);
						ImGui::TableNextColumn();
						const std::string padName = captureGamepad == static_cast<int>(i) ? "Press button..." : SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(project.input.gamepad[i]));
						if (ImGui::Button(padName.empty() ? "Bind button" : padName.c_str(), ImVec2(-1, 0))) captureGamepad = static_cast<int>(i);
						ImGui::PopID();
					}
					ImGui::EndTable();
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Appearance")) {
				ImGui::SliderFloat("UI scale", &uiScale, 0.70f, 1.25f, "%.2fx");
				ImGui::SliderFloat("Editor line spacing", &editorLineSpacing, 1.0f, 1.5f, "%.2fx");
				if (ImGui::Button("Reset appearance")) { uiScale = 0.88f; editorLineSpacing = 1.08f; }
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	};

	editor.SetLineDecorator(-3.25f, [&](TextEditor::Decorator& decorator) {
		const bool active = editor.GetUserData(decorator.line) != nullptr;
		const bool resolved = validBuild && !sourceDirty && currentBuild.sourceLineToAddress.contains(decorator.line + 1);
		const float size = decorator.height - 2.0f;
		const ImVec2 position = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("breakpoint", ImVec2(decorator.width, size));
		if (ImGui::IsItemClicked()) {
			editor.SetUserData(decorator.line, active ? nullptr : reinterpret_cast<void*>(1));
			synchronizeBreakpoints();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(active ? "Remove breakpoint" : "Add breakpoint");
		ImDrawList* draw = ImGui::GetWindowDrawList();
		const ImVec2 center(position.x + decorator.width * 0.5f, position.y + size * 0.5f);
		draw->AddLine(ImVec2(position.x + decorator.width - 2.0f, position.y),
			ImVec2(position.x + decorator.width - 2.0f, position.y + size), IM_COL32(78, 84, 91, 150));
		if (active) draw->AddCircleFilled(center, size * 0.29f,
			resolved ? IM_COL32(236, 72, 82, 255) : IM_COL32(226, 163, 54, 255), 20);
		else draw->AddCircle(center, size * 0.24f, IM_COL32(125, 132, 140, 210), 20, 1.5f);
	});

	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			SDL_Window* eventWindow = SDL_GetWindowFromEvent(&event);
			const bool eventInSettings = settingsWindow && eventWindow == settingsWindow;
			ImGui::SetCurrentContext(eventInSettings ? settingsContext : mainContext);
			ImGui_ImplSDL3_ProcessEvent(&event);
			const bool wantCaptureKeyboard = ImGui::GetIO().WantCaptureKeyboard;
			ImGui::SetCurrentContext(mainContext);
			if (event.type == SDL_EVENT_QUIT) running = false;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
				if (eventInSettings) settingsOpen = false;
				else if (eventWindow == window) running = false;
			}
			if (event.type == SDL_EVENT_GAMEPAD_ADDED && !gamepad) gamepad = SDL_OpenGamepad(event.gdevice.which);
			if (event.type == SDL_EVENT_GAMEPAD_REMOVED && gamepad && SDL_GetGamepadID(gamepad) == event.gdevice.which) {
				SDL_CloseGamepad(gamepad);
				gamepad = nullptr;
				gamepadHeld.fill(false);
			}
			if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
				const bool pressed = event.type == SDL_EVENT_KEY_DOWN;
				if (pressed && captureKeyboard >= 0) {
					project.input.keyboard[static_cast<size_t>(captureKeyboard)] = static_cast<int32_t>(event.key.key);
					captureKeyboard = -1;
				} else {
					size_t button = 0;
					if (bindingMatches(project.input.keyboard, static_cast<int>(event.key.key), button)) {
						if (!eventInSettings && (!pressed || !wantCaptureKeyboard)) {
							keyboardHeld[button] = pressed;
							if (pressed && !event.key.repeat) machine.queueInput(static_cast<uint16_t>(project.input.keyboard[button] & 0xFFFF));
						}
					} else if (!eventInSettings && pressed && !event.key.repeat && !wantCaptureKeyboard) {
						machine.queueInput(static_cast<uint16_t>(event.key.key & 0xFFFF));
					}
				}
			}
			if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
				const bool pressed = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
				if (pressed && captureGamepad >= 0) {
					project.input.gamepad[static_cast<size_t>(captureGamepad)] = static_cast<int32_t>(event.gbutton.button);
					captureGamepad = -1;
				} else {
					size_t button = 0;
					if (bindingMatches(project.input.gamepad, event.gbutton.button, button)) {
						gamepadHeld[button] = pressed;
						if (pressed) machine.queueInput(static_cast<uint16_t>(project.input.keyboard[button] & 0xFFFF));
					}
				}
			}
		}

		uint16_t heldButtons = 0;
		for (size_t i = 0; i < console::ConsoleButtonCount; ++i) {
			const bool held = keyboardHeld[i] || gamepadHeld[i] || screenHeld[i];
			if (held) heldButtons |= static_cast<uint16_t>(1u << i);
		}
		machine.setInputButtons(heldButtons);
		screenHeld.fill(false);

		const uint64_t currentTick = SDL_GetTicksNS();
		const double elapsedSeconds = std::min(0.25, static_cast<double>(currentTick - lastTick) / 1'000'000'000.0);
		lastTick = currentTick;
		const console::MachineState stateBeforeExecution = machine.state();
		if (machine.state() == console::MachineState::Running) {
			const uint64_t executionDeadline = SDL_GetTicksNS() + 4'000'000;
			if (unlimitedSpeed) {
				cycleAccumulator = 0.0;
				do {
					const uint64_t cyclesBefore = machine.cycles();
					machine.runForCycles(200'000);
					if (machine.cycles() == cyclesBefore) break;
				} while (machine.state() == console::MachineState::Running && SDL_GetTicksNS() < executionDeadline);
			} else {
				cycleAccumulator += elapsedSeconds * static_cast<double>(machine.profile().clockHz) * executionSpeed;
				while (machine.state() == console::MachineState::Running && cycleAccumulator >= 1.0 &&
					SDL_GetTicksNS() < executionDeadline) {
					const uint64_t executionBudget = std::min<uint64_t>(static_cast<uint64_t>(cycleAccumulator), 200'000);
					const uint64_t cyclesBefore = machine.cycles();
					machine.runForCycles(executionBudget);
					const uint64_t consumedCycles = machine.cycles() - cyclesBefore;
					if (consumedCycles == 0) break;
					cycleAccumulator = std::max(0.0, cycleAccumulator - static_cast<double>(consumedCycles));
				}
				const double maximumBacklog = static_cast<double>(machine.profile().clockHz) * executionSpeed * 0.25;
				cycleAccumulator = std::min(cycleAccumulator, maximumBacklog);
			}
		}
		if (stateBeforeExecution == console::MachineState::Running && machine.state() != console::MachineState::Running) centerExecutionLine = true;
		if (audioStream) {
			std::vector<float> samples = machine.drainAudioSamples();
			if (unlimitedSpeed) {
				if (SDL_GetAudioStreamQueued(audioStream) > 0) SDL_ClearAudioStream(audioStream);
			} else if (!samples.empty()) {
				std::vector<float> playbackSamples = console::resampleAudioForSpeed(samples, executionSpeed);
				constexpr int MaximumQueuedAudioBytes = 48'000 * static_cast<int>(sizeof(float)) / 2;
				if (SDL_GetAudioStreamQueued(audioStream) < MaximumQueuedAudioBytes) {
					SDL_PutAudioStreamData(audioStream, playbackSamples.data(),
						static_cast<int>(playbackSamples.size() * sizeof(float)));
				}
			}
		}

		const auto& profile = machine.profile();
		if (!displayTexture || textureWidth != profile.displayWidth || textureHeight != profile.displayHeight) {
			if (displayTexture) SDL_DestroyTexture(displayTexture);
			displayTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
				static_cast<int>(profile.displayWidth), static_cast<int>(profile.displayHeight));
			textureWidth = profile.displayWidth;
			textureHeight = profile.displayHeight;
			SDL_SetTextureScaleMode(displayTexture, SDL_SCALEMODE_NEAREST);
		}
		SDL_UpdateTexture(displayTexture, nullptr, machine.framebuffer().data(), static_cast<int>(profile.displayWidth * sizeof(uint32_t)));

		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		ImGui::GetStyle().FontScaleMain = uiScale;
		editor.SetLineSpacing(editorLineSpacing);
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("Project")) {
				if (ImGui::MenuItem("Settings")) settingsOpen = true;
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View")) {
				if (ImGui::MenuItem("Settings")) settingsOpen = true;
				if (ImGui::MenuItem("Reset workspace layout")) resetWorkspace = true;
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
		const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		if (resetWorkspace) {
			ImGui::DockBuilderRemoveNode(dockspaceId);
			resetWorkspace = false;
		}
		configureDefaultDocking(dockspaceId, ImGui::GetMainViewport()->Size);

		int executionLine = 0;
		if (validBuild) {
			const auto mapped = currentBuild.addressToSourceLine.find(machine.pc());
			if (mapped != currentBuild.addressToSourceLine.end()) executionLine = mapped->second;
		}
		editor.ClearMarkers();
		if (executionLine > 0) {
			editor.AddMarker(executionLine - 1, IM_COL32(177, 132, 42, 255), IM_COL32(177, 132, 42, 50), "Current instruction", "Current instruction");
			if (centerExecutionLine) editor.ScrollToLine(executionLine - 1, TextEditor::Scroll::alignMiddle);
		}
		centerExecutionLine = false;

		ImGui::Begin("Assembly Editor", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x - 180.0f));
		ImGui::InputText("##source", sourcePath.data(), sourcePath.size());
		ImGui::SameLine();
		if (ImGui::Button("Load")) {
			std::string text;
			if (readTextFile(sourcePath.data(), text)) {
				editor.SetText(text);
				clearEditorBreakpoints(editor);
				sourceDirty = true;
				validBuild = false;
				diagnostics = "Loaded source file.\n";
			} else diagnostics = "Could not load source file.\n";
		}
		ImGui::SameLine();
		if (ImGui::Button("Save")) diagnostics = writeTextFile(sourcePath.data(), editor.GetText()) ? "Saved source file.\n" : "Could not save source file.\n";
		if (ImGui::Button("Run")) buildAndLoad(true);
		ImGui::SameLine();
		if (ImGui::Button("Continue")) machine.run();
		ImGui::SameLine();
		if (ImGui::Button("Pause")) { machine.pause(); centerExecutionLine = true; }
		ImGui::SameLine();
		if (ImGui::Button("Step")) {
			if ((!validBuild || sourceDirty) && !buildAndLoad(false)) {
				// Diagnostics are already populated.
			} else {
				machine.step();
				centerExecutionLine = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) { machine.reset(); synchronizeBreakpoints(); centerExecutionLine = true; }
		ImGui::SameLine();
		ImGui::TextDisabled("%s%s", validBuild ? "Built" : "Not built", sourceDirty ? " - modified" : "");
		ImGui::SetNextItemWidth(125.0f);
		if (ImGui::SliderFloat("Speed", &executionSpeed, 0.05f, 10.0f, "%.2fx", ImGuiSliderFlags_Logarithmic)) {
			unlimitedSpeed = false;
			applySpeedPreference();
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Unlimited", &unlimitedSpeed)) applySpeedPreference();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run as fast as the host allows; audio output is disabled.");
		editor.Render("Source editor", ImGui::GetContentRegionAvail(), false);
		ImGui::End();

		ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		std::ostringstream speedLabelStream;
		if (unlimitedSpeed) speedLabelStream << "Unlimited";
		else speedLabelStream << std::fixed << std::setprecision(2) << executionSpeed << 'x';
		const std::string speedLabel = speedLabelStream.str();
		ImGui::Text("%s | %ux%u RGB332 @ %u Hz", profile.name.c_str(), profile.displayWidth,
			profile.displayHeight, profile.framesPerSecond);
		ImGui::TextDisabled("%s | Frame %llu | Scanline %u/%u (%.0f%%)", speedLabel.c_str(),
			static_cast<unsigned long long>(machine.frames()),
			machine.currentScanline() + 1, profile.displayHeight, machine.scanlineProgress() * 100.0);
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float controlsHeight = 132.0f;
		const float scale = std::min(available.x / profile.displayWidth, std::max(1.0f, available.y - controlsHeight) / profile.displayHeight);
		const ImVec2 imageSize(profile.displayWidth * scale, profile.displayHeight * scale);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (available.x - imageSize.x) * 0.5f));
		ImGui::Image(reinterpret_cast<ImTextureID>(displayTexture), imageSize);
		ImGui::Separator();
		const ImVec2 controlsOrigin = ImGui::GetCursorScreenPos();
		const float controlsWidth = ImGui::GetContentRegionAvail().x;
		const float padCell = 30.0f;
		const ImVec2 padOrigin(controlsOrigin.x + 12.0f, controlsOrigin.y + 8.0f);
		ImDrawList* controlsDraw = ImGui::GetWindowDrawList();
		controlsDraw->AddRectFilled(ImVec2(padOrigin.x + padCell, padOrigin.y), ImVec2(padOrigin.x + padCell * 2, padOrigin.y + padCell * 3), IM_COL32(46, 52, 58, 255), 5.0f);
		controlsDraw->AddRectFilled(ImVec2(padOrigin.x, padOrigin.y + padCell), ImVec2(padOrigin.x + padCell * 3, padOrigin.y + padCell * 2), IM_COL32(46, 52, 58, 255), 5.0f);
		auto directionButton = [&](size_t index, const char* id, ImVec2 position) {
			ImGui::SetCursorScreenPos(position);
			ImGui::PushID(id);
			ImGui::InvisibleButton("direction", ImVec2(padCell, padCell));
			screenHeld[index] = ImGui::IsItemActive();
			if (ImGui::IsItemActivated()) machine.queueInput(static_cast<uint16_t>(project.input.keyboard[index] & 0xFFFF));
			if (screenHeld[index]) controlsDraw->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(112, 127, 135, 255), 4.0f);
			ImGui::PopID();
		};
		directionButton(0, "up", ImVec2(padOrigin.x + padCell, padOrigin.y));
		directionButton(1, "down", ImVec2(padOrigin.x + padCell, padOrigin.y + padCell * 2));
		directionButton(2, "left", ImVec2(padOrigin.x, padOrigin.y + padCell));
		directionButton(3, "right", ImVec2(padOrigin.x + padCell * 2, padOrigin.y + padCell));
		const ImU32 arrowColor = IM_COL32(190, 196, 200, 255);
		controlsDraw->AddTriangleFilled(ImVec2(padOrigin.x + padCell * 1.5f, padOrigin.y + 7), ImVec2(padOrigin.x + padCell + 8, padOrigin.y + 21), ImVec2(padOrigin.x + padCell * 2 - 8, padOrigin.y + 21), arrowColor);
		controlsDraw->AddTriangleFilled(ImVec2(padOrigin.x + padCell * 1.5f, padOrigin.y + padCell * 3 - 7), ImVec2(padOrigin.x + padCell + 8, padOrigin.y + padCell * 2 + 9), ImVec2(padOrigin.x + padCell * 2 - 8, padOrigin.y + padCell * 2 + 9), arrowColor);
		controlsDraw->AddTriangleFilled(ImVec2(padOrigin.x + 7, padOrigin.y + padCell * 1.5f), ImVec2(padOrigin.x + 21, padOrigin.y + padCell + 8), ImVec2(padOrigin.x + 21, padOrigin.y + padCell * 2 - 8), arrowColor);
		controlsDraw->AddTriangleFilled(ImVec2(padOrigin.x + padCell * 3 - 7, padOrigin.y + padCell * 1.5f), ImVec2(padOrigin.x + padCell * 2 + 9, padOrigin.y + padCell + 8), ImVec2(padOrigin.x + padCell * 2 + 9, padOrigin.y + padCell * 2 - 8), arrowColor);

		const float actionX = controlsOrigin.x + controlsWidth - 112.0f;
		ImGui::SetCursorScreenPos(ImVec2(actionX, controlsOrigin.y + 42.0f));
		if (circularControl("button-b", "B", ImVec2(48, 48), IM_COL32(80, 145, 190, 255), screenHeld[5])) machine.queueInput(static_cast<uint16_t>(project.input.keyboard[5] & 0xFFFF));
		ImGui::SetCursorScreenPos(ImVec2(actionX + 54.0f, controlsOrigin.y + 16.0f));
		if (circularControl("button-a", "A", ImVec2(48, 48), IM_COL32(203, 78, 91, 255), screenHeld[4])) machine.queueInput(static_cast<uint16_t>(project.input.keyboard[4] & 0xFFFF));

		const float centerX = controlsOrigin.x + controlsWidth * 0.5f - 68.0f;
		ImGui::SetCursorScreenPos(ImVec2(centerX, controlsOrigin.y + 58.0f));
		if (slimControl("button-select", "Select", ImVec2(62, 23), screenHeld[7])) machine.queueInput(static_cast<uint16_t>(project.input.keyboard[7] & 0xFFFF));
		ImGui::SetCursorScreenPos(ImVec2(centerX + 72.0f, controlsOrigin.y + 58.0f));
		if (slimControl("button-start", "Start", ImVec2(62, 23), screenHeld[6])) machine.queueInput(static_cast<uint16_t>(project.input.keyboard[6] & 0xFFFF));
		ImGui::SetCursorScreenPos(ImVec2(controlsOrigin.x, controlsOrigin.y + 106.0f));
		ImGui::Dummy(ImVec2(controlsWidth, 1));
		ImGui::End();

		ImGui::Begin("Debugger");
		ImGui::Text("State: %s", stateName(machine.state()));
		ImGui::Text("PC %04X   SP %04X", machine.pc(), machine.sp());
		ImGui::Text("Cycles %llu   Frames %llu", static_cast<unsigned long long>(machine.cycles()), static_cast<unsigned long long>(machine.frames()));
		ImGui::Text("Banks  RAM %u  VRAM %u  ROM %u  Storage %u", machine.ramBank(), machine.vramBank(), machine.romBank(), machine.storageBank());
		if (executionLine > 0) ImGui::TextColored(ImVec4(0.95f, 0.76f, 0.30f, 1), "Source line %d", executionLine);
		if (machine.state() == console::MachineState::Faulted) ImGui::TextColored(ImVec4(1, 0.35f, 0.35f, 1), "%s", machine.faultMessage().c_str());
		if (ImGui::CollapsingHeader("Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::BeginTable("registers", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
				for (size_t i = 0; i < machine.registers().size(); ++i) {
					ImGui::TableNextColumn();
					uint16_t value = machine.registers()[i];
					ImGui::PushID(static_cast<int>(i));
					ImGui::SetNextItemWidth(-1);
					std::string label = "R" + std::to_string(i) + "##register";
					if (ImGui::InputScalar(label.c_str(), ImGuiDataType_U16, &value, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal)) machine.registers()[i] = value;
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
		}
		if (ImGui::CollapsingHeader("Disassembly", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (int instruction = -4; instruction <= 7; ++instruction) {
				const uint16_t address = static_cast<uint16_t>(machine.pc() + instruction * 8);
				const std::string decoded = console::disassemble(machine.peekInstruction(address));
				if (address == machine.pc()) ImGui::TextColored(ImVec4(0.95f, 0.76f, 0.30f, 1), "> %04X  %s", address, decoded.c_str());
				else ImGui::Text("  %04X  %s", address, decoded.c_str());
			}
		}
		ImGui::End();

		ImGui::Begin("Inspector");
		if (ImGui::BeginTabBar("memory-tabs")) {
			if (ImGui::BeginTabItem("RAM")) { renderHexBuffer("ram-view", machine.ram(), ramAddress); ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("VRAM")) {
				if (ImGui::BeginTabBar("vram-tabs")) {
					if (ImGui::BeginTabItem("Hex")) { renderHexBuffer("vram-view", machine.vram(), vramAddress); ImGui::EndTabItem(); }
					if (ImGui::BeginTabItem("RGB332")) { renderVramPixels(machine.vram(), vramPixelOffset); ImGui::EndTabItem(); }
					if (ImGui::BeginTabItem("Framebuffer")) {
						const float width = ImGui::GetContentRegionAvail().x;
						ImGui::Image(reinterpret_cast<ImTextureID>(displayTexture), ImVec2(width, width * profile.displayHeight / profile.displayWidth));
						ImGui::EndTabItem();
					}
					ImGui::EndTabBar();
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("ROM")) { renderHexBuffer("rom-view", machine.rom(), romAddress); ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Storage")) { renderHexBuffer("storage-view", machine.storage(), storageAddress); ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Devices")) {
				const std::array<std::pair<uint16_t, const char*>, 25> devices = {{
					{0xFF00, "RAM bank"}, {0xFF01, "VRAM bank"}, {0xFF02, "ROM bank"}, {0xFF03, "Storage bank"},
					{0xFF10, "PPU control"}, {0xFF11, "PPU status"}, {0xFF13, "Sprite count low"}, {0xFF14, "Sprite count high"},
					{0xFF20, "Input queued"}, {0xFF21, "Input data"}, {0xFF22, "Buttons low"}, {0xFF23, "Buttons high"},
					{0xFF30, "Profile"}, {0xFF31, "Fault"}, {0xFF40, "Audio channel"}, {0xFF41, "Audio control"},
					{0xFF42, "Frequency low"}, {0xFF43, "Frequency high"}, {0xFF44, "Volume"},
					{0xFF50, "Milliseconds high"}, {0xFF51, "Milliseconds low"},
					{0xFF52, "Frames high"}, {0xFF53, "Frames low"},
					{0xFF54, "Cycles high"}, {0xFF55, "Cycles low"}
				}};
				if (ImGui::BeginTable("devices", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
					ImGui::TableSetupColumn("Address"); ImGui::TableSetupColumn("Register"); ImGui::TableSetupColumn("Value");
					ImGui::TableHeadersRow();
					for (const auto& [address, name] : devices) {
						ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("%04X", address);
						ImGui::TableNextColumn(); ImGui::TextUnformatted(name);
						ImGui::TableNextColumn(); ImGui::Text("%02X", machine.readByte(address));
					}
					ImGui::EndTable();
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();

		ImGui::Begin("Output");
		if (ImGui::BeginTabBar("output-tabs")) {
			if (ImGui::BeginTabItem("Build")) { ImGui::TextWrapped("%s", diagnostics.c_str()); ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Program")) {
				if (ImGui::Button("Clear")) machine.clearOutput();
				ImGui::Separator(); ImGui::TextWrapped("%s", machine.output().c_str()); ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();


		ImGui::Render();
		SDL_SetRenderDrawColor(renderer, 14, 16, 19, 255);
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);

		if (settingsOpen && !settingsWindow) createSettingsWindow();
		if (!settingsOpen && settingsWindow) destroySettingsWindow();
		if (settingsContext) {
			ImGui::SetCurrentContext(settingsContext);
			ImGui_ImplSDLRenderer3_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();
			ImGui::GetStyle().FontScaleMain = uiScale;
			ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
			ImGui::Begin("Settings Content", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
			renderSettingsContent();
			ImGui::End();
			ImGui::Render();
			SDL_SetRenderDrawColor(settingsRenderer, 18, 20, 23, 255);
			SDL_RenderClear(settingsRenderer);
			ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), settingsRenderer);
			SDL_RenderPresent(settingsRenderer);
			ImGui::SetCurrentContext(mainContext);
		}
	}

	preferences.executionSpeed = executionSpeed;
	preferences.unlimited = unlimitedSpeed;
	saveDesktopPreferences(preferencesPath, preferences);
	destroySettingsWindow();
	ImGui::SetCurrentContext(mainContext);
	if (gamepad) SDL_CloseGamepad(gamepad);
	if (displayTexture) SDL_DestroyTexture(displayTexture);
	if (audioStream) SDL_DestroyAudioStream(audioStream);
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
