#ifndef PPU_DEBUG_HPP
#define PPU_DEBUG_HPP

#include <string>
#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL_opengl.h>

#include "types.hpp"
#include "gba.hpp"

extern GameBoyAdvance GBA;
extern GLuint debugTexture;

extern bool showLayerView;
void layerViewWindow();

#endif