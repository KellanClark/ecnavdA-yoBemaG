#ifndef PPU_DEBUG_HPP
#define PPU_DEBUG_HPP

#include <string>
#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>

#include "types.hpp"
#include "gba.hpp"

extern GameBoyAdvance GBA;

void initPpuDebug();

extern bool showLayerView;
void layerViewWindow();
extern bool showTiles;
void tilesWindow();
extern bool showPalette;
void paletteWindow();

#endif