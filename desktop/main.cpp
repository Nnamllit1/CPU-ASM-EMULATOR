#include "../console/console_machine.h"
#include "../console/asset_import.h"
#include "../console/disassembler.h"
#include "../console/project.h"
#include "../console/source_builder.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

constexpr size_t EditorCapacity = 256 * 1024;

const char* defaultSource = R"asm(; AI console experiment: RGB332 color bars.
; VRAM begins at 0xC000 and is selected through 0xFF01.

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

    movi r0, 1
    stbi r0, 0xFF12
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

} // namespace

int main(int, char**) {
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		std::cerr << "SDL initialization failed: " << SDL_GetError() << "\n";
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("CPU ASM Console Experiment", 1440, 900, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
	if (!window || !renderer) {
		std::cerr << "SDL window/renderer creation failed: " << SDL_GetError() << "\n";
		if (renderer) SDL_DestroyRenderer(renderer);
		if (window) SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);
	SDL_AudioSpec audioSpec{};
	audioSpec.format = SDL_AUDIO_F32;
	audioSpec.channels = 1;
	audioSpec.freq = 48'000;
	SDL_AudioStream* audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, nullptr, nullptr);
	if (audioStream) SDL_ResumeAudioStreamDevice(audioStream);

	console::ConsoleProject project;
	console::ConsoleMachine machine(console::pocketProfile());
	std::array<char, EditorCapacity> sourceBuffer{};
	std::strncpy(sourceBuffer.data(), defaultSource, sourceBuffer.size() - 1);
	std::array<char, 512> sourcePath{};
	std::array<char, 512> projectPath{};
	std::array<char, 512> assetPath{};
	std::array<char, 512> storagePath{};
	std::strncpy(sourcePath.data(), "examples/console/color-bars.asm", sourcePath.size() - 1);
	std::strncpy(projectPath.data(), "examples/console/color-bars.console.json", projectPath.size() - 1);
	std::strncpy(assetPath.data(), "examples/console/checker.ppm", assetPath.size() - 1);
	std::strncpy(storagePath.data(), "console-storage.sav", storagePath.size() - 1);
	std::string diagnostics = "Select Build to assemble and load the program.\n";
	int selectedProfile = 0;
	int breakpointAddress = 0;
	SDL_Texture* displayTexture = nullptr;
	uint32_t textureWidth = 0;
	uint32_t textureHeight = 0;
	bool running = true;
	uint64_t lastTick = SDL_GetTicksNS();
	double cycleAccumulator = 0.0;

	auto configureMachine = [&]() {
		project.profile = static_cast<console::ProfileId>(selectedProfile);
		const console::HardwareProfile profile = project.profile == console::ProfileId::Studio ?
			project.studio : console::profileFor(project.profile);
		std::string error;
		if (!machine.configure(profile, error)) diagnostics = "Profile error: " + error + "\n";
		cycleAccumulator = 0.0;
	};

	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) running = false;
			if (event.type == SDL_EVENT_KEY_DOWN && !io.WantCaptureKeyboard) {
				machine.queueInput(static_cast<uint16_t>(event.key.key & 0xFFFF));
			}
		}

		const uint64_t currentTick = SDL_GetTicksNS();
		const double elapsedSeconds = std::min(0.25, static_cast<double>(currentTick - lastTick) / 1'000'000'000.0);
		lastTick = currentTick;
		if (machine.state() == console::MachineState::Running) {
			cycleAccumulator += elapsedSeconds * static_cast<double>(machine.profile().clockHz);
			const uint64_t availableCycles = static_cast<uint64_t>(cycleAccumulator);
			const uint64_t executionBudget = std::min<uint64_t>(availableCycles, 200'000);
			machine.runForCycles(executionBudget);
			cycleAccumulator -= static_cast<double>(executionBudget);
			cycleAccumulator = std::min(cycleAccumulator, 400'000.0);
		}
		if (audioStream) {
			std::vector<float> samples = machine.drainAudioSamples();
			if (!samples.empty()) SDL_PutAudioStreamData(audioStream, samples.data(), static_cast<int>(samples.size() * sizeof(float)));
		}

		const auto& profile = machine.profile();
		if (!displayTexture || textureWidth != profile.displayWidth || textureHeight != profile.displayHeight) {
			if (displayTexture) SDL_DestroyTexture(displayTexture);
			displayTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_STREAMING, static_cast<int>(profile.displayWidth), static_cast<int>(profile.displayHeight));
			textureWidth = profile.displayWidth;
			textureHeight = profile.displayHeight;
			SDL_SetTextureScaleMode(displayTexture, SDL_SCALEMODE_NEAREST);
		}
		SDL_UpdateTexture(displayTexture, nullptr, machine.framebuffer().data(), static_cast<int>(profile.displayWidth * sizeof(uint32_t)));

		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
		ImGui::Begin("Console Workspace", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

		if (ImGui::BeginTable("workspace", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch, 0.50f);
			ImGui::TableSetupColumn("Display", ImGuiTableColumnFlags_WidthStretch, 0.28f);
			ImGui::TableSetupColumn("Debugger", ImGuiTableColumnFlags_WidthStretch, 0.22f);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Assembly Editor");
			ImGui::InputText("Source", sourcePath.data(), sourcePath.size());
			ImGui::SameLine();
			if (ImGui::Button("Load")) {
				std::string text;
				if (readTextFile(sourcePath.data(), text) && text.size() < sourceBuffer.size()) {
					std::fill(sourceBuffer.begin(), sourceBuffer.end(), 0);
					std::copy(text.begin(), text.end(), sourceBuffer.begin());
					diagnostics = "Loaded source file.\n";
				} else diagnostics = "Could not load source file or it is too large.\n";
			}
			ImGui::SameLine();
			if (ImGui::Button("Save")) {
				diagnostics = writeTextFile(sourcePath.data(), sourceBuffer.data()) ? "Saved source file.\n" : "Could not save source file.\n";
			}
			ImGui::InputTextMultiline("##editor", sourceBuffer.data(), sourceBuffer.size(),
				ImVec2(-1, ImGui::GetContentRegionAvail().y * 0.72f), ImGuiInputTextFlags_AllowTabInput);
			if (ImGui::Button("Build and Load")) {
				console::SourceBuildResult build = console::buildAssemblySource(sourceBuffer.data());
				diagnostics = build.diagnostics;
				if (build.success) {
					std::string error;
					if (!machine.loadRom(build.rom, build.entryPoint, error)) diagnostics += error + "\n";
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Run")) machine.run();
			ImGui::SameLine();
			if (ImGui::Button("Pause")) machine.pause();
			ImGui::SameLine();
			if (ImGui::Button("Step")) machine.step();
			ImGui::SameLine();
			if (ImGui::Button("Reset")) machine.reset();
			ImGui::SeparatorText("Build Output");
			ImGui::TextWrapped("%s", diagnostics.c_str());

			ImGui::TableNextColumn();
			ImGui::Text("%s - %ux%u RGB332", profile.name.c_str(), profile.displayWidth, profile.displayHeight);
			const float availableWidth = ImGui::GetContentRegionAvail().x;
			const float scale = availableWidth / static_cast<float>(profile.displayWidth);
			ImGui::Image((ImTextureID)(intptr_t)displayTexture,
				ImVec2(profile.displayWidth * scale, profile.displayHeight * scale));
			ImGui::SeparatorText("Hardware Profile");
			const char* profiles[] = { "Pocket Color", "Home 16", "Studio" };
			if (ImGui::Combo("Profile", &selectedProfile, profiles, 3)) configureMachine();
			if (selectedProfile == 2) {
				int64_t clock = static_cast<int64_t>(project.studio.clockHz);
				const int64_t minimumClock = 1;
				const int64_t maximumClock = 1'000'000'000;
				if (ImGui::SliderScalar("Clock Hz", ImGuiDataType_S64, &clock,
					&minimumClock, &maximumClock, "%lld", ImGuiSliderFlags_Logarithmic)) {
					project.studio.clockHz = static_cast<uint64_t>(clock);
				}
				int width = static_cast<int>(project.studio.displayWidth);
				int height = static_cast<int>(project.studio.displayHeight);
				if (ImGui::SliderInt("Width", &width, 64, 1920)) project.studio.displayWidth = static_cast<uint32_t>(width);
				if (ImGui::SliderInt("Height", &height, 64, 1080)) project.studio.displayHeight = static_cast<uint32_t>(height);
				int ramMiB = static_cast<int>(project.studio.ramBytes / (1024 * 1024));
				int romMiB = static_cast<int>(project.studio.romBytes / (1024 * 1024));
				int vramMiB = static_cast<int>(project.studio.vramBytes / (1024 * 1024));
				int storageMiB = static_cast<int>(project.studio.storageBytes / (1024 * 1024));
				if (ImGui::SliderInt("RAM MiB", &ramMiB, 1, 256)) project.studio.ramBytes = static_cast<size_t>(ramMiB) * 1024 * 1024;
				if (ImGui::SliderInt("ROM MiB", &romMiB, 1, 512)) project.studio.romBytes = static_cast<size_t>(romMiB) * 1024 * 1024;
				if (ImGui::SliderInt("VRAM MiB", &vramMiB, 1, 256)) project.studio.vramBytes = static_cast<size_t>(vramMiB) * 1024 * 1024;
				if (ImGui::SliderInt("Storage MiB", &storageMiB, 0, 256)) project.studio.storageBytes = static_cast<size_t>(storageMiB) * 1024 * 1024;
				int sprites = static_cast<int>(project.studio.maxSprites);
				int scanlineSprites = static_cast<int>(project.studio.spritesPerScanline);
				int paletteColors = static_cast<int>(project.studio.paletteColors);
				int audioChannels = static_cast<int>(project.studio.audioChannels);
				if (ImGui::SliderInt("Sprites", &sprites, 0, 4096)) project.studio.maxSprites = static_cast<uint32_t>(sprites);
				if (ImGui::SliderInt("Sprites/scanline", &scanlineSprites, 0, 1024)) project.studio.spritesPerScanline = static_cast<uint32_t>(scanlineSprites);
				if (ImGui::SliderInt("Palette colors", &paletteColors, 2, 256)) project.studio.paletteColors = static_cast<uint32_t>(paletteColors);
				if (ImGui::SliderInt("Audio channels", &audioChannels, 0, 64)) project.studio.audioChannels = static_cast<uint32_t>(audioChannels);
				if (ImGui::Button("Apply Studio Settings")) configureMachine();
			}
			ImGui::InputText("Project", projectPath.data(), projectPath.size());
			if (ImGui::Button("Load Project")) {
				std::string error;
				if (console::loadProject(projectPath.data(), project, error)) {
					selectedProfile = static_cast<int>(project.profile);
					std::strncpy(sourcePath.data(), project.sourcePath.c_str(), sourcePath.size() - 1);
					std::strncpy(storagePath.data(), project.storagePath.c_str(), storagePath.size() - 1);
					std::strncpy(assetPath.data(), project.assetPath.c_str(), assetPath.size() - 1);
					configureMachine();
					diagnostics = "Project loaded.\n";
				} else diagnostics = error + "\n";
			}
			ImGui::SameLine();
			if (ImGui::Button("Save Project")) {
				project.sourcePath = sourcePath.data();
				project.storagePath = storagePath.data();
				project.assetPath = assetPath.data();
				std::string error;
				diagnostics = console::saveProject(projectPath.data(), project, error) ? "Project saved.\n" : error + "\n";
			}
			ImGui::SeparatorText("Persistent Storage");
			ImGui::InputText("Storage file", storagePath.data(), storagePath.size());
			if (ImGui::Button("Load Storage")) {
				std::string error;
				diagnostics = machine.loadStorageFile(storagePath.data(), error) ? "Storage loaded.\n" : error + "\n";
			}
			ImGui::SameLine();
			if (ImGui::Button("Save Storage")) {
				std::string error;
				diagnostics = machine.saveStorageFile(storagePath.data(), error) ? "Storage saved.\n" : error + "\n";
			}
			ImGui::SeparatorText("Asset Import");
			ImGui::InputText("PPM image", assetPath.data(), assetPath.size());
			if (ImGui::Button("Import to VRAM")) {
				console::ImportedImage image;
				std::string error;
				if (console::importPpmRgb332(assetPath.data(), image, error) && machine.loadVram(image.rgb332, 0, error)) {
					diagnostics = "Imported " + std::to_string(image.width) + "x" + std::to_string(image.height) + " RGB332 image.\n";
				} else diagnostics = error + "\n";
			}
			if (ImGui::CollapsingHeader("Hardware Reference")) {
				ImGui::TextWrapped("RAM bank FF00, VRAM bank FF01, ROM bank FF02, storage bank FF03. "
					"PPU control FF10, present FF12, sprite count FF13-FF14. "
					"Input FF20-FF21. Audio channel FF40, enable FF41, frequency FF42-FF43, volume FF44. "
					"Framebuffer pixels are RGB332 bytes at the start of VRAM; PPU control bit 1 selects 8x8 tile mode.");
			}

			ImGui::TableNextColumn();
			ImGui::Text("State: %s", stateName(machine.state()));
			ImGui::Text("PC: %04X  SP: %04X", machine.pc(), machine.sp());
			ImGui::Text("Cycles: %llu", static_cast<unsigned long long>(machine.cycles()));
			ImGui::Text("Frames: %llu", static_cast<unsigned long long>(machine.frames()));
			ImGui::Text("Banks R:%u V:%u ROM:%u S:%u", machine.ramBank(), machine.vramBank(), machine.romBank(), machine.storageBank());
			ImGui::Text("Limits: %u sprites, %u/scanline, %u colors, %u audio channels",
				profile.maxSprites, profile.spritesPerScanline, profile.paletteColors, profile.audioChannels);
			if (machine.state() == console::MachineState::Faulted) {
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", machine.faultMessage().c_str());
			}
			ImGui::InputInt("Breakpoint", &breakpointAddress, 8, 64, ImGuiInputTextFlags_CharsHexadecimal);
			if (ImGui::Button("Add Breakpoint")) machine.addBreakpoint(static_cast<uint16_t>(breakpointAddress));
			ImGui::SameLine();
			if (ImGui::Button("Clear")) machine.clearBreakpoints();
			if (!machine.breakpoints().empty()) {
				ImGui::TextUnformatted("Active:");
				ImGui::SameLine();
				for (uint16_t address : machine.breakpoints()) {
					ImGui::Text("%04X", address);
					ImGui::SameLine();
				}
				ImGui::NewLine();
			}
			ImGui::SeparatorText("Registers");
			if (ImGui::BeginTable("registers", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				for (size_t i = 0; i < machine.registers().size(); ++i) {
					ImGui::TableNextColumn();
					uint16_t value = machine.registers()[i];
					ImGui::PushID(static_cast<int>(i));
					ImGui::Text("R%02zu", i);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(-1);
					if (ImGui::InputScalar("##register", ImGuiDataType_U16, &value, nullptr, nullptr, "%04X",
						ImGuiInputTextFlags_CharsHexadecimal)) machine.registers()[i] = value;
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			ImGui::SeparatorText("Disassembly");
			for (int instruction = -3; instruction <= 5; ++instruction) {
				const uint16_t address = static_cast<uint16_t>(machine.pc() + instruction * 8);
				const uint64_t encoded = machine.peekInstruction(address);
				if (address == machine.pc()) ImGui::TextColored(ImVec4(1, 0.85f, 0.2f, 1), "> %04X  %s", address, console::disassemble(encoded).c_str());
				else ImGui::Text("  %04X  %s", address, console::disassemble(encoded).c_str());
			}
			ImGui::SeparatorText("RAM 0000-00FF");
			for (int row = 0; row < 16; ++row) {
				std::ostringstream line;
				line << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << row * 16 << ": ";
				for (int column = 0; column < 16; ++column) {
					line << std::setw(2) << static_cast<int>(machine.readByte(static_cast<uint16_t>(row * 16 + column))) << ' ';
				}
				ImGui::TextUnformatted(line.str().c_str());
			}
			ImGui::SeparatorText("Program Output");
			ImGui::TextWrapped("%s", machine.output().c_str());
			ImGui::EndTable();
		}
		ImGui::End();

		ImGui::Render();
		SDL_SetRenderDrawColor(renderer, 15, 17, 22, 255);
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

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
