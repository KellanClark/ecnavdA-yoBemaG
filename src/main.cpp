
#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"
#include <SDL2/SDL.h>
#include <GL/gl3w.h>
#include <nfd.hpp>

#include "gba.hpp"

// Argument Variables
bool argRomGiven;
std::filesystem::path argRomFilePath;
bool argBiosGiven;
std::filesystem::path argBiosFilePath;
bool argLogConsole;

constexpr auto cexprHash(const char *str, std::size_t v = 0) noexcept -> std::size_t {
	return (*str == 0) ? v : 31 * cexprHash(str + 1) + *str;
}

// Graphics
SDL_Window* window;
uint32_t pixels[240 * 160];
GLuint lcdTexture;

// ImGui Windows
void mainMenuBar();
bool showRomInfo;
void romInfoWindow();
bool showNoBios;
void noBiosWindow();

void romFileDialog();
void biosFileDialog();

// Everything else
volatile bool updateScreen;
GameBoyAdvance GBA;
std::thread emuThread(&GameBoyAdvance::run, GBA);
int loadRom();

int main(int argc, char *argv[]) {
	// Parse arguments
	argRomGiven = false;
	argBiosGiven = false;
	argBiosFilePath = "";
	argLogConsole = false;
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
		case cexprHash("--log-to-console"):
			argLogConsole = true;
			break;
		default:
			if (i == 1) {
				argRomGiven = true;
				argRomFilePath = argv[i];
			} else {
				printf("Unknown argument:  %s\n", argv[i]);
				return -1;
			}
		}
	}
	if (argRomGiven) {
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
	if (gl3wInit()) {
		printf("Failed to initialize OpenGL loader!\n");
		return 1;
	}

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
	bool showDemoWindow = true;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Create image for main display
	glGenTextures(1, &lcdTexture);
	glBindTexture(GL_TEXTURE_2D, lcdTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // GL_LINEAR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	memset(pixels, 0, sizeof(pixels));

	bool quit = false;
	SDL_Event event;
	while (!quit) {
		//unsigned int frameStartTicks = SDL_GetTicks();
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

		if (updateScreen) {
			glBindTexture(GL_TEXTURE_2D, lcdTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 160, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixels);
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

		// Console Screen
		{
			ImGui::Begin("Game Boy Advance Screen");

			ImGui::Text("%.1f FPS", io.Framerate);
			ImGui::Image((void*)(intptr_t)lcdTexture, ImVec2(240 * 2, 160 * 2));

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

	emuThread.detach();

	// ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	// SDL
	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

int loadRom() {
	GBA.reset();
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

		if (ImGui::MenuItem("Save", "Ctrl+S", false, false)) {
			GBA.save();
		}

		ImGui::Separator();
		ImGui::MenuItem("ROM Info", NULL, &showRomInfo, argRomGiven);

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Emulation")) {
		ImGui::MenuItem("tmp");

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Debug")) {
		ImGui::MenuItem("tmp");

		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

void romInfoWindow() {
	ImGui::Begin("ROM Info", &showRomInfo);

	ImGui::Text("ROM File:  %s\n", argRomFilePath.c_str());
	ImGui::Text("BIOS File:  %s\n", argBiosFilePath.c_str());

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