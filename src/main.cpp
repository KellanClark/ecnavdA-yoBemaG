
#include <cstdio>
#include <list>
#include <numeric>

#include "SDL_audio.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_memory_editor.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL_opengl.h>
#include <nfd.hpp>

#include "gba.hpp"
#include "types.hpp"

// Argument Variables
bool argRomGiven;
std::filesystem::path argRomFilePath;
bool argBiosGiven;
std::filesystem::path argBiosFilePath;
bool recordSound;
bool argWavGiven;
std::filesystem::path argWavFilePath;

constexpr auto cexprHash(const char *str, std::size_t v = 0) noexcept -> std::size_t {
	return (*str == 0) ? v : 31 * cexprHash(str + 1) + *str;
}

// Create emulator thread
GameBoyAdvance GBA;
std::thread emuThread(&GBACPU::run, std::ref(GBA.cpu));

// Graphics
SDL_Window* window;
GLuint lcdTexture;

// ImGui Windows
void mainMenuBar();
bool showDemoWindow;
bool showRomInfo;
void romInfoWindow();
bool showNoBios;
void noBiosWindow();
bool showCpuDebug;
void cpuDebugWindow();
bool showSystemLog;
void systemLogWindow();
bool showMemEditor;
void memEditorWindow();
#include "ppudebug.hpp"

void romFileDialog();
void biosFileDialog();
MemoryEditor memEditor;
ImU8 memEditorRead(const ImU8* data, size_t off);
void memEditorWrite(ImU8* data, size_t off, ImU8 d);
bool memEditorHighlight(const ImU8* data, size_t off);

// Input
SDL_Scancode keymap[10] = {
	SDL_SCANCODE_X, // Button A
	SDL_SCANCODE_Z, // Button B
	SDL_SCANCODE_BACKSPACE, // Select
	SDL_SCANCODE_RETURN, // Start
	SDL_SCANCODE_RIGHT, // Right
	SDL_SCANCODE_LEFT, // Left
	SDL_SCANCODE_UP, // Up
	SDL_SCANCODE_DOWN, // Down
	SDL_SCANCODE_S, // Button R
	SDL_SCANCODE_A // Button L
};
u16 lastJoypad;

// Audio stuff
bool syncToAudio;
SDL_AudioSpec desiredAudioSpec, audioSpec;
SDL_AudioDeviceID audioDevice;
std::vector<i16> wavFileData;
std::ofstream wavFileStream;
void audioCallback(void *userdata, uint8_t *stream, int len);

// Everything else
std::atomic<bool> quit = false;
int loadRom();

int main(int argc, char *argv[]) {
	// Parse arguments
	argRomGiven = false;
	argBiosGiven = false;
	argBiosFilePath = "";
	recordSound = false;
	argWavGiven = false;
	for (int i = 1; i < argc; i++) {
		switch (cexprHash(argv[i])) {
		case cexprHash("--rom"):
			if (argc == (++i)) {
				printf("Not enough arguments for flag --rom\n");
				return -1;
			}
			argRomGiven = true;
			argRomFilePath = argv[i];
			break;
		case cexprHash("--bios"):
			if (argc == ++i) {
				printf("Not enough arguments for flag --bios\n");
				return -1;
			}
			argBiosGiven = true;
			argBiosFilePath = argv[i];
			break;
		case cexprHash("--record"):
			if (argc == ++i) {
				printf("Not enough arguments for flag --record\n");
				return -1;
			}
			recordSound = true;
			argWavGiven = true;
			argWavFilePath = argv[i];
			break;
		default:
			if (i == 1) {
				argRomGiven = true;
				argRomFilePath = argv[i];
			} else {
				printf("Unknown argument:  %s\n", argv[i]);
				return -1;
			}
			break;
		}
	}
	if (argRomGiven && argBiosGiven) {
		if (loadRom() == -1) {
			return -1;
		}
	}

	// Setup SDL and OpenGL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf("Error: %s\n", SDL_GetError());
		return -1;
	}
	const char* glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	std::string windowName = "ecnavdA yoBemaG - ";
	windowName += argRomFilePath;
	window = SDL_CreateWindow(windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(0);

	// Setup Audio
	desiredAudioSpec = {
		.freq = 32768,
		.format = AUDIO_S16,
		.channels = 2,
		.samples = 1024,
		.callback = audioCallback,
		.userdata = NULL
	};
	audioDevice = SDL_OpenAudioDevice(NULL, 0, &desiredAudioSpec, &audioSpec, 0);
	SDL_PauseAudioDevice(audioDevice, 0);

	// Setup ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	 // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	  // Enable Gamepad Controls
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init(glsl_version);
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	memEditor.ReadFn = memEditorRead;
	memEditor.WriteFn = memEditorWrite;
	memEditor.HighlightFn = memEditorHighlight;

	// Create image for main display
	glGenTextures(1, &lcdTexture);
	glBindTexture(GL_TEXTURE_2D, lcdTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // GL_LINEAR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	initPpuDebug();

	NFD::Guard nfdGuard;

	u32 emuThreadTicks = 0;
	u32 emuThreadTicksLast = 0;
	std::list<u32> emuThreadTicksBuff;
	emuThreadTicksBuff.resize(60 * 5);
	u32 emuThreadTicksBuffTotal = 0;
	SDL_Event event;
	while (!quit) {
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);

			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
				quit = true;
			switch (event.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.mod & KMOD_CTRL) {
					switch (event.key.keysym.sym) {
					case SDLK_s:
						GBA.save();
						break;
					case SDLK_o:
						if (event.key.keysym.mod & KMOD_SHIFT) {
							biosFileDialog();
						} else {
							romFileDialog();
						}
						break;
					}
				}
				break;
			}
		}
		// Joypad inputs
		const u8 *currentKeyStates = SDL_GetKeyboardState(NULL);
		u16 currentJoypad = 0;
		for (int i = 0; i < 10; i++) {
			if (currentKeyStates[keymap[i]])
				currentJoypad |= 1 << i;
		}
		if (currentJoypad != lastJoypad) {
			GBA.cpu.addThreadEvent(GBACPU::UPDATE_KEYINPUT, ~currentJoypad);
			lastJoypad = currentJoypad;
		}

		if (GBA.ppu.updateScreen) {
			// Update FPS count
			emuThreadTicksLast = emuThreadTicks;
			emuThreadTicks = SDL_GetTicks();
			emuThreadTicksBuffTotal -= emuThreadTicksBuff.back();
			emuThreadTicksBuff.pop_back();
			emuThreadTicksBuff.push_front(emuThreadTicks - emuThreadTicksLast);
			emuThreadTicksBuffTotal += emuThreadTicksBuff.front();

			glBindTexture(GL_TEXTURE_2D, lcdTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 240, 160, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, GBA.ppu.framebuffer);
			GBA.ppu.updateScreen = false;
		}

		/* Draw ImGui Stuff */
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		mainMenuBar();

		if (showDemoWindow)
			ImGui::ShowDemoWindow(&showDemoWindow);
		if (showRomInfo)
			romInfoWindow();
		if (showNoBios)
			noBiosWindow();
		if (showCpuDebug)
			cpuDebugWindow();
		if (showSystemLog)
			systemLogWindow();
		if (showMemEditor)
			memEditorWindow();
		if (showLayerView)
			layerViewWindow();
		if (showTiles)
			tilesWindow();
		if (showPalette)
			paletteWindow();

		// Console Screen
		{
			ImGui::Begin("Game Boy Advance Screen");

			ImGui::Text("Rendering Thread:  %.1f FPS", io.Framerate);
			ImGui::Text("Emulator Thread:   %.1f FPS", 1000.0/((float)emuThreadTicksBuffTotal / emuThreadTicksBuff.size()));
			ImGui::Image((void*)(intptr_t)lcdTexture, ImVec2(240 * 3, 160 * 3));

			ImGui::End();
		}

		/* Rendering */
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}

	GBA.cpu.addThreadEvent(GBACPU::STOP, 0);
	emuThread.detach();

	// WAV file
	if (argWavGiven) {
		struct  __attribute__((__packed__)) {
			char riffStr[4] = {'R', 'I', 'F', 'F'};
			unsigned int fileSize = 0;
			char waveStr[4] = {'W', 'A', 'V', 'E'};
			char fmtStr[4] = {'f', 'm', 't', ' '};
			unsigned int subchunk1Size = 16;
			unsigned short audioFormat = 1;
			unsigned short numChannels = 2;
			unsigned int sampleRate = audioSpec.freq;
			unsigned int byteRate = audioSpec.freq * sizeof(i16) * 2;
			unsigned short blockAlign = 4;
			unsigned short bitsPerSample = sizeof(i16) * 8;
			char dataStr[4] = {'d', 'a', 't', 'a'};
			unsigned int subchunk2Size = 0;
		} wavHeaderData;
		wavFileStream.open(argWavFilePath, std::ios::binary | std::ios::trunc);
		wavHeaderData.subchunk2Size = (wavFileData.size() * sizeof(i16));
		wavHeaderData.fileSize = sizeof(wavHeaderData) - 8 + wavHeaderData.subchunk2Size;
		wavFileStream.write(reinterpret_cast<const char*>(&wavHeaderData), sizeof(wavHeaderData));
		wavFileStream.write(reinterpret_cast<const char*>(wavFileData.data()), wavFileData.size() * sizeof(i16));
		wavFileStream.close();
	}

	// ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	// SDL
	SDL_CloseAudioDevice(audioDevice);
	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

void audioCallback(void *userdata, uint8_t *stream, int len) {
	while ((GBA.apu.sampleBufferIndex * 2) < len) {
		if (quit)
			return;
	}

	if (recordSound) {
		wavFileData.insert(wavFileData.end(), GBA.apu.sampleBuffer.begin(), GBA.apu.sampleBuffer.begin() + GBA.apu.sampleBufferIndex);
	}
	GBA.apu.sampleBufferIndex = 0;

	memcpy(stream, &GBA.apu.sampleBuffer, len);

	GBA.cpu.running = true;
}

int loadRom() {
	GBA.cpu.addThreadEvent(GBACPU::RESET);

	if (GBA.loadRom(argRomFilePath, argBiosFilePath))
		return -1;

	return 0;
}

void mainMenuBar() {
	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Load ROM", "Ctrl+O")) {
			romFileDialog();
		}

		if (ImGui::MenuItem("Load Bios", "Ctrl+Shift+O")) {
			biosFileDialog();
		}

		if (ImGui::MenuItem("Save", "Ctrl+S", false, argRomGiven)) {
			GBA.save();
		}

		ImGui::Separator();
		ImGui::MenuItem("ROM Info", NULL, &showRomInfo, argRomGiven);

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Emulation")) {
		if (GBA.cpu.running) {
			if (ImGui::MenuItem("Pause"))
				GBA.cpu.addThreadEvent(GBACPU::STOP, 0);
		} else {
			if (ImGui::MenuItem("Unpause"))
				GBA.cpu.addThreadEvent(GBACPU::START);
		}
		if (ImGui::MenuItem("Reset")) {
			GBA.cpu.addThreadEvent(GBACPU::RESET);
			GBA.cpu.addThreadEvent(GBACPU::START);
		}
		
		ImGui::Separator();
		if (ImGui::BeginMenu("Audio Channels")) {
			ImGui::MenuItem("Channel 1", NULL, &GBA.apu.ch1OverrideEnable);
			ImGui::MenuItem("Channel 2", NULL, &GBA.apu.ch2OverrideEnable);
			ImGui::MenuItem("Channel 3", NULL, &GBA.apu.ch3OverrideEnable);
			ImGui::MenuItem("Channel 4", NULL, &GBA.apu.ch4OverrideEnable);
			ImGui::MenuItem("Channel A", NULL, &GBA.apu.chAOverrideEnable);
			ImGui::MenuItem("Channel B", NULL, &GBA.apu.chBOverrideEnable);

			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Debug")) {
		ImGui::MenuItem("Debug CPU", NULL, &showCpuDebug);
		ImGui::MenuItem("System Log", NULL, &showSystemLog);
		ImGui::MenuItem("Memory Editor", NULL, &showMemEditor);
		ImGui::MenuItem("Inspect Layers", NULL, &showLayerView);
		ImGui::MenuItem("View Tiles", NULL, &showTiles);
		ImGui::MenuItem("View Palettes", NULL, &showPalette);
		ImGui::Separator();
		ImGui::MenuItem("ImGui Demo", NULL, &showDemoWindow);

		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

void romInfoWindow() {
	ImGui::Begin("ROM Info", &showRomInfo);

	std::string saveTypeString;
	switch (GBA.saveType) {
	case GameBoyAdvance::UNKNOWN:
		saveTypeString = "Unknown";
		break;
	case GameBoyAdvance::EEPROM_512B:
		saveTypeString = "512 byte EEPROM";
		break;
	case GameBoyAdvance::EEPROM_8K:
		saveTypeString = "8 kilobyte EEPROM";
		break;
	case GameBoyAdvance::SRAM_32K:
		saveTypeString = "32 kilobyte SRAM";
		break;
	case GameBoyAdvance::FLASH_128K:
		saveTypeString = "128 kilobyte Flash";
		break;
	}

	ImGui::Text("ROM File:  %s", argRomFilePath.c_str());
	ImGui::Text("BIOS File:  %s", argBiosFilePath.c_str());
	ImGui::Text("Save Type:  %s", saveTypeString.c_str());

	ImGui::End();
}

void noBiosWindow() {
	// Center window
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5, 0.5));
	ImGui::Begin("No BIOS Selected");

	ImGui::Text("You have chosen a ROM, but no BIOS.\nWould you like to start now or wait until a BIOS is selected?");

	if (ImGui::Button("Wait"))
		showNoBios = false;
	ImGui::SameLine();
	if (ImGui::Button("Continue")) {
		showNoBios = false;
		loadRom();
	}

	ImGui::End();
}

void cpuDebugWindow() {
	ImGui::Begin("Debug CPU", &showCpuDebug);

	if (ImGui::Button("Reset"))
		GBA.cpu.addThreadEvent(GBACPU::RESET);
	ImGui::SameLine();
	if (GBA.cpu.running) {
		if (ImGui::Button("Pause"))
			GBA.cpu.addThreadEvent(GBACPU::STOP, 0);
	} else {
		if (ImGui::Button("Unpause"))
			GBA.cpu.addThreadEvent(GBACPU::START);
	}
	
	ImGui::Spacing();
	if (ImGui::Button("Step")) {
		// Add events the hard way so mutex doesn't have to be unlocked
		GBA.cpu.threadQueueMutex.lock();
		GBA.cpu.threadQueue.push(GBACPU::threadEvent{GBACPU::START, 0, nullptr});
		GBA.cpu.threadQueue.push(GBACPU::threadEvent{GBACPU::STOP, 1, nullptr});
		GBA.cpu.threadQueueMutex.unlock();
	}

	ImGui::Separator();
	std::string tmp = GBA.cpu.disassemble(GBA.cpu.reg.R[15] - (GBA.cpu.reg.thumbMode ? 4 : 8), GBA.cpu.pipelineOpcode3, GBA.cpu.reg.thumbMode);
	ImGui::Text("Current Opcode:  %s", tmp.c_str());
	ImGui::Spacing();
	ImGui::Text("r0:  %08X", GBA.cpu.reg.R[0]);
	ImGui::Text("r1:  %08X", GBA.cpu.reg.R[1]);
	ImGui::Text("r2:  %08X", GBA.cpu.reg.R[2]);
	ImGui::Text("r3:  %08X", GBA.cpu.reg.R[3]);
	ImGui::Text("r4:  %08X", GBA.cpu.reg.R[4]);
	ImGui::Text("r5:  %08X", GBA.cpu.reg.R[5]);
	ImGui::Text("r6:  %08X", GBA.cpu.reg.R[6]);
	ImGui::Text("r7:  %08X", GBA.cpu.reg.R[7]);
	ImGui::Text("r8:  %08X", GBA.cpu.reg.R[8]);
	ImGui::Text("r9:  %08X", GBA.cpu.reg.R[9]);
	ImGui::Text("r10: %08X", GBA.cpu.reg.R[10]);
	ImGui::Text("r11: %08X", GBA.cpu.reg.R[11]);
	ImGui::Text("r12: %08X", GBA.cpu.reg.R[12]);
	ImGui::Text("r13: %08X", GBA.cpu.reg.R[13]);
	ImGui::Text("r14: %08X", GBA.cpu.reg.R[14]);
	ImGui::Text("r15: %08X", GBA.cpu.reg.R[15]);
	ImGui::Text("CPSR: %08X", GBA.cpu.reg.CPSR);

	ImGui::Spacing();
	bool imeTmp = GBA.cpu.IME;
	ImGui::Checkbox("IME", &imeTmp);
	ImGui::SameLine();
	ImGui::Text("IE: %04X", GBA.cpu.IE);
	ImGui::SameLine();
	ImGui::Text("IF: %04X", GBA.cpu.IF);

	ImGui::Spacing();
	if (ImGui::Button("Show System Log"))
		showSystemLog = true;

	ImGui::End();
}

void systemLogWindow() {
	static bool shouldAutoscroll = true;

	ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("System Log", &showSystemLog);

	ImGui::Checkbox("Trace Instructions", (bool *)&GBA.cpu.traceInstructions);
	ImGui::SameLine();
	ImGui::Checkbox("Log Interrupts", (bool *)&GBA.cpu.logInterrupts);
	ImGui::SameLine();
	ImGui::Checkbox("Log Flash Commands", (bool *)&GBA.logFlash);
	ImGui::SameLine();
	ImGui::Checkbox("Log DMAs", (bool *)&GBA.dma.logDma);

	ImGui::Checkbox("Auto-scroll", &shouldAutoscroll);
	ImGui::SameLine();
	if (ImGui::Button("Save Log")) {
		std::ofstream logFileStream{"log", std::ios::trunc};
		logFileStream << GBA.log.str();
		logFileStream.close();
	}

	if (ImGui::TreeNode("Diassembler Options")) {
		ImGui::Checkbox("Show AL Condition", (bool *)&GBA.cpu.disassemblerOptions.showALCondition);
		ImGui::Checkbox("Always Show S Bit", (bool *)&GBA.cpu.disassemblerOptions.alwaysShowSBit);
		ImGui::Checkbox("Show Operands in Hex", (bool *)&GBA.cpu.disassemblerOptions.printOperandsHex);
		ImGui::Checkbox("Show Addresses in Hex", (bool *)&GBA.cpu.disassemblerOptions.printAddressesHex);
		ImGui::Checkbox("Simplify Register Names", (bool *)&GBA.cpu.disassemblerOptions.simplifyRegisterNames);
		ImGui::Checkbox("Simplify LDM and STM to PUSH and POP", (bool *)&GBA.cpu.disassemblerOptions.simplifyPushPop);
		ImGui::Checkbox("Use Alternative Stack Suffixes for LDM and STM", (bool *)&GBA.cpu.disassemblerOptions.ldmStmStackSuffixes);
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Separator();

	if (ImGui::TreeNode("Log")) {
		ImGui::BeginChild("Debug CPU", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::TextUnformatted(GBA.log.str().c_str());
		if (shouldAutoscroll)
			ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
		ImGui::TreePop();
	}

	ImGui::End();
}

void romFileDialog() {
	nfdfilteritem_t filter[1] = {{"Game Boy Advance ROM", "gba,bin"}};
	NFD::UniquePath nfdRomFilePath;
	nfdresult_t nfdResult = NFD::OpenDialog(nfdRomFilePath, filter, 1);
	if (nfdResult == NFD_OKAY) {
		argRomGiven = true;
		argRomFilePath = nfdRomFilePath.get();
		if (!argBiosGiven) {
			showNoBios = true;
		} else {
			loadRom();
		}
	} else if (nfdResult != NFD_CANCEL) {
		printf("Error: %s\n", NFD::GetError());
	}
}

void biosFileDialog() {
	nfdfilteritem_t filter[1] = {{"Game Boy Advance BIOS", "bin"}};
	NFD::UniquePath nfdBiosFilePath;
	nfdresult_t nfdResult = NFD::OpenDialog(nfdBiosFilePath, filter, 1);
	if (nfdResult == NFD_OKAY) {
		argBiosGiven = true;
		argBiosFilePath = nfdBiosFilePath.get();
		showNoBios = false;
		if (argRomGiven)
			loadRom();
	} else if (nfdResult != NFD_CANCEL) {
		printf("Error: %s\n", NFD::GetError());
	}
}

void memEditorWindow() {
	ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Memory Editor", &showMemEditor);

	// I *may* have straight coppied this text from GBATEK
	if (ImGui::BeginCombo("Location", "Jump to Memory Range")) {
		if (ImGui::MenuItem("BIOS - System ROM (0x0000000-0x0003FFF)"))
			memEditor.GotoAddrAndHighlight(0x0000000, 0x0000000);
		if (ImGui::MenuItem("WRAM - On-board Work RAM (0x2000000-0x203FFFF)"))
			memEditor.GotoAddrAndHighlight(0x2000000, 0x2000000);
		if (ImGui::MenuItem("WRAM - On-chip Work RAM (0x3000000-0x3007FFF)"))
			memEditor.GotoAddrAndHighlight(0x3000000, 0x3000000);
		if (ImGui::MenuItem("I/O Registers (0x4000000-0x4000209)"))
			memEditor.GotoAddrAndHighlight(0x4000000, 0x4000000);
		if (ImGui::MenuItem("BG/OBJ Palette RAM (0x5000000-0x50003FF)"))
			memEditor.GotoAddrAndHighlight(0x5000000, 0x5000000);
		if (ImGui::MenuItem("VRAM - Video RAM (0x6000000-0x6017FFF)"))
			memEditor.GotoAddrAndHighlight(0x6000000, 0x6000000);
		if (ImGui::MenuItem("OAM - OBJ Attributes (0x7000000-0x70003FF)"))
			memEditor.GotoAddrAndHighlight(0x7000000, 0x7000000);
		if (ImGui::MenuItem("Game Pak ROM (0x8000000-0x9FFFFFF)"))
			memEditor.GotoAddrAndHighlight(0x8000000, 0x9000000);
		if (ImGui::MenuItem("Game Pak SRAM (0xE000000-0xE00FFFF)"))
			memEditor.GotoAddrAndHighlight(0xE000000, 0xE000000);

		ImGui::EndCombo();
	}

	memEditor.DrawContents(NULL, 0x10000000);

	ImGui::End();
}

ImU8 memEditorRead(const ImU8* data, size_t off) {
	return GBA.read<u8>((u32)off);
}

void memEditorWrite(ImU8* data, size_t off, ImU8 d) {
	GBA.write<u8>((u32)off, d);
}

bool memEditorHighlight(const ImU8* data, size_t off) {
	switch (off) {
	case 0x0000000 ... 0x0003FFF:
	case 0x2000000 ... 0x203FFFF:
	case 0x3000000 ... 0x3007FFF:
	case 0x4000000 ... 0x4000209:
	case 0x5000000 ... 0x50003FF:
	case 0x6000000 ... 0x6017FFF:
	case 0x7000000 ... 0x70003FF:
	case 0x8000000 ... 0x9FFFFFF:
	case 0xE000000 ... 0xE00FFFF:
		return true;
	default:
		return false;
	}
}