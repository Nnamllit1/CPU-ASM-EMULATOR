#include "../console/asset_import.h"
#include "../console/console_machine.h"
#include "../console/disassembler.h"
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
	ImGui::DockBuilderDockWindow("Project Settings", bottom);
	ImGui::DockBuilderFinish(dockspaceId);
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
	std::strncpy(sourcePath.data(), "examples/console/color-bars.asm", sourcePath.size() - 1);
	std::strncpy(projectPath.data(), "examples/console/color-bars.console.json", projectPath.size() - 1);
	std::strncpy(assetPath.data(), "examples/console/checker.ppm", assetPath.size() - 1);
	std::strncpy(storagePath.data(), "console-storage.sav", storagePath.size() - 1);

	std::string diagnostics = "Run assembles the current source, loads a fresh ROM, and starts it.\n";
	console::SourceBuildResult currentBuild;
	bool sourceDirty = true;
	bool validBuild = false;
	bool centerExecutionLine = false;
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

	editor.SetLineDecorator(-2.0f, [&](TextEditor::Decorator& decorator) {
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
		if (active) {
			ImGui::GetWindowDrawList()->AddCircleFilled(
				ImVec2(position.x + decorator.width * 0.5f, position.y + size * 0.5f), size * 0.28f,
				resolved ? IM_COL32(232, 83, 91, 255) : IM_COL32(214, 158, 67, 255));
		}
	});

	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) running = false;
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
						if (!pressed || !io.WantCaptureKeyboard) {
							keyboardHeld[button] = pressed;
							if (pressed && !event.key.repeat) machine.queueInput(static_cast<uint16_t>(project.input.keyboard[button] & 0xFFFF));
						}
					} else if (pressed && !event.key.repeat && !io.WantCaptureKeyboard) {
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
			cycleAccumulator += elapsedSeconds * static_cast<double>(machine.profile().clockHz);
			const uint64_t availableCycles = static_cast<uint64_t>(cycleAccumulator);
			const uint64_t executionBudget = std::min<uint64_t>(availableCycles, 200'000);
			machine.runForCycles(executionBudget);
			cycleAccumulator -= static_cast<double>(executionBudget);
			cycleAccumulator = std::min(cycleAccumulator, 400'000.0);
		}
		if (stateBeforeExecution == console::MachineState::Running && machine.state() != console::MachineState::Running) centerExecutionLine = true;
		if (audioStream) {
			std::vector<float> samples = machine.drainAudioSamples();
			if (!samples.empty()) SDL_PutAudioStreamData(audioStream, samples.data(), static_cast<int>(samples.size() * sizeof(float)));
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
		const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		configureDefaultDocking(dockspaceId, ImGui::GetMainViewport()->Size);

		int executionLine = 0;
		if (validBuild) {
			const auto mapped = currentBuild.addressToSourceLine.find(machine.pc());
			if (mapped != currentBuild.addressToSourceLine.end()) executionLine = mapped->second;
		}
		editor.ClearMarkers();
		for (int line : editorBreakpointLines(editor)) {
			editor.AddMarker(line - 1, IM_COL32(120, 38, 46, 255), IM_COL32(95, 28, 34, 75), "Breakpoint", "Breakpoint");
		}
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
		editor.Render("Source editor", ImGui::GetContentRegionAvail(), false);
		ImGui::End();

		ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::Text("%s  |  %ux%u RGB332", profile.name.c_str(), profile.displayWidth, profile.displayHeight);
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float controlsHeight = 132.0f;
		const float scale = std::min(available.x / profile.displayWidth, std::max(1.0f, available.y - controlsHeight) / profile.displayHeight);
		const ImVec2 imageSize(profile.displayWidth * scale, profile.displayHeight * scale);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (available.x - imageSize.x) * 0.5f));
		ImGui::Image(reinterpret_cast<ImTextureID>(displayTexture), imageSize);
		ImGui::Separator();
		const ImVec2 buttonSize(46, 34);
		auto consoleButton = [&](size_t index, const char* label) {
			ImGui::PushID(static_cast<int>(index));
			const bool clicked = ImGui::Button(label, buttonSize);
			screenHeld[index] = ImGui::IsItemActive();
			if (clicked) machine.queueInput(static_cast<uint16_t>(project.input.keyboard[index] & 0xFFFF));
			ImGui::PopID();
		};
		ImGui::BeginGroup();
		ImGui::Indent(48); consoleButton(0, "Up"); ImGui::Unindent(48);
		consoleButton(2, "Left"); ImGui::SameLine(); consoleButton(3, "Right");
		ImGui::Indent(48); consoleButton(1, "Down"); ImGui::Unindent(48);
		ImGui::EndGroup();
		ImGui::SameLine(0, 38);
		ImGui::BeginGroup();
		consoleButton(7, "Select"); ImGui::SameLine(); consoleButton(6, "Start");
		ImGui::EndGroup();
		ImGui::SameLine(0, 38);
		ImGui::BeginGroup();
		consoleButton(5, "B"); ImGui::SameLine(); consoleButton(4, "A");
		ImGui::EndGroup();
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
				const std::array<std::pair<uint16_t, const char*>, 19> devices = {{
					{0xFF00, "RAM bank"}, {0xFF01, "VRAM bank"}, {0xFF02, "ROM bank"}, {0xFF03, "Storage bank"},
					{0xFF10, "PPU control"}, {0xFF11, "PPU status"}, {0xFF13, "Sprite count low"}, {0xFF14, "Sprite count high"},
					{0xFF20, "Input queued"}, {0xFF21, "Input data"}, {0xFF22, "Buttons low"}, {0xFF23, "Buttons high"},
					{0xFF30, "Profile"}, {0xFF31, "Fault"}, {0xFF40, "Audio channel"}, {0xFF41, "Audio control"},
					{0xFF42, "Frequency low"}, {0xFF43, "Frequency high"}, {0xFF44, "Volume"}
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

		ImGui::Begin("Project Settings");
		const char* profiles[] = { "Pocket Color", "Home 16", "Studio" };
		if (ImGui::Combo("Profile", &selectedProfile, profiles, 3)) configureMachine();
		if (selectedProfile == static_cast<int>(console::ProfileId::Studio) && ImGui::CollapsingHeader("Studio hardware")) {
			int64_t clock = static_cast<int64_t>(project.studio.clockHz);
			const int64_t minimumClock = 1;
			const int64_t maximumClock = 1'000'000'000;
			if (ImGui::SliderScalar("Clock Hz", ImGuiDataType_S64, &clock, &minimumClock, &maximumClock, "%lld", ImGuiSliderFlags_Logarithmic)) project.studio.clockHz = static_cast<uint64_t>(clock);
			int width = static_cast<int>(project.studio.displayWidth);
			int height = static_cast<int>(project.studio.displayHeight);
			if (ImGui::SliderInt("Width", &width, 64, 1920)) project.studio.displayWidth = static_cast<uint32_t>(width);
			if (ImGui::SliderInt("Height", &height, 64, 1080)) project.studio.displayHeight = static_cast<uint32_t>(height);
			int ramMiB = static_cast<int>(project.studio.ramBytes / (1024 * 1024));
			int romMiB = static_cast<int>(project.studio.romBytes / (1024 * 1024));
			int vramMiB = static_cast<int>(project.studio.vramBytes / (1024 * 1024));
			int storageMiB = static_cast<int>(project.studio.storageBytes / (1024 * 1024));
			ImGui::SliderInt("RAM MiB", &ramMiB, 1, 256);
			ImGui::SliderInt("ROM MiB", &romMiB, 1, 512);
			ImGui::SliderInt("VRAM MiB", &vramMiB, 1, 256);
			ImGui::SliderInt("Storage MiB", &storageMiB, 0, 256);
			project.studio.ramBytes = static_cast<size_t>(ramMiB) * 1024 * 1024;
			project.studio.romBytes = static_cast<size_t>(romMiB) * 1024 * 1024;
			project.studio.vramBytes = static_cast<size_t>(vramMiB) * 1024 * 1024;
			project.studio.storageBytes = static_cast<size_t>(storageMiB) * 1024 * 1024;
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
			if (ImGui::Button("Apply hardware settings")) configureMachine();
		}
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
		ImGui::SeparatorText("Storage and assets");
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
		ImGui::SeparatorText("Input bindings");
		ImGui::TextDisabled("Controller: %s", gamepad ? SDL_GetGamepadName(gamepad) : "not connected");
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
		ImGui::End();

		ImGui::Render();
		SDL_SetRenderDrawColor(renderer, 14, 16, 19, 255);
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

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
