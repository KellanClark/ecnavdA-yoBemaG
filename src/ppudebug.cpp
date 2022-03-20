
#include "ppudebug.hpp"
#include "SDL_opengl.h"
#include "imgui.h"

#define convertColor(x) (x | 0x8000)

GLuint debugTexture;
u16 debugBuffer[512 * 512];
GLuint debugTilesTexture;
u16 debugTilesBuffer[256 * 512];

void initPpuDebug() {
	// Create image for debug display
	glGenTextures(1, &debugTexture);
	glBindTexture(GL_TEXTURE_2D, debugTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // GL_LINEAR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &debugTilesTexture);
	glBindTexture(GL_TEXTURE_2D, debugTilesTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // GL_LINEAR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

enum bgLayer {
	BG0_REGULAR,
	BG1_REGULAR,
	BG2_REGULAR,
	BG3_REGULAR,
	MODE3_BG2,
	MODE4_BG2,
	MODE4_BG2_FLIPPED,
	MODE5_BG2,
	MODE5_BG2_FLIPPED
};
struct layerInfoEntry {
    bgLayer enumValue;
	std::string name;
	int xSize;
	int ySize;
};
layerInfoEntry layerInfo[] = {
	{BG0_REGULAR, "Mode 0/1, Background 0, Regular", 512, 512},
	{BG1_REGULAR, "Mode 0/1, Background 1, Regular", 512, 512},
	{BG2_REGULAR, "Mode 0, Background 2, Regular", 512, 512},
	{BG3_REGULAR, "Mode 0, Background 3, Regular", 512, 512},
	{MODE3_BG2, "Mode 3, Background 2", 240, 160},
	{MODE4_BG2, "Mode 4, Background 2", 240, 160},
	{MODE4_BG2_FLIPPED, "Mode 4, Background 2, Flipped", 240, 160},
	{MODE5_BG2, "Mode 5, Background 2", 160, 128},
	{MODE5_BG2_FLIPPED, "Mode 5, Background 2, Flipped", 160, 128}
};
const int layerEntriesNum = sizeof(layerInfo)/sizeof(layerInfoEntry);
int currentlySelectedLayer = 0;

template <int bgNum, int size>
int calculateTilemapIndex(int x, int y) {
	int baseBlock;
	switch (bgNum) {
	case 0: baseBlock = GBA.ppu.bg0ScreenBaseBlock; break;
	case 1: baseBlock = GBA.ppu.bg1ScreenBaseBlock; break;
	case 2: baseBlock = GBA.ppu.bg2ScreenBaseBlock; break;
	case 3: baseBlock = GBA.ppu.bg3ScreenBaseBlock; break;
	}

	int offset;
	switch (size) {
	case 0: // 256x256
		return (baseBlock * 0x800) + (((y % 256) / 8) * 64) + (((x % 256) / 8) * 2);
	case 1: // 512x256
		offset = (baseBlock + ((x >> 8) & 1)) * 0x800;
		return offset + (((y % 256) / 8) * 64) + (((x % 256) / 8) * 2);
	case 2: // 256x512
		return (baseBlock * 0x800) + (((y % 512) / 8) * 64) + (((x % 256) / 8) * 2);
	case 3: // 512x512
		offset = ((baseBlock + ((x >> 8) & 1)) * 0x800) + (16 * (y & 0x100));
		return offset + (((y % 256) / 8) * 64) + (((x % 256) / 8) * 2);
	}
}
constexpr int (*tilemapIndexLUTDebug[])(int, int) = {
	&calculateTilemapIndex<0, 0>,
	&calculateTilemapIndex<0, 1>,
	&calculateTilemapIndex<0, 2>,
	&calculateTilemapIndex<0, 3>,
	&calculateTilemapIndex<1, 0>,
	&calculateTilemapIndex<1, 1>,
	&calculateTilemapIndex<1, 2>,
	&calculateTilemapIndex<1, 3>,
	&calculateTilemapIndex<2, 0>,
	&calculateTilemapIndex<2, 1>,
	&calculateTilemapIndex<2, 2>,
	&calculateTilemapIndex<2, 3>,
	&calculateTilemapIndex<3, 0>,
	&calculateTilemapIndex<3, 1>,
	&calculateTilemapIndex<3, 2>,
	&calculateTilemapIndex<3, 3>
};

int screenXSize;
int screenYSize;
void drawDebugLayer(bgLayer type, u16 *buffer) {
	screenXSize = layerInfo[currentlySelectedLayer].xSize;
	screenYSize = layerInfo[currentlySelectedLayer].ySize;
	switch (type) {
	case BG0_REGULAR: // Shamefuly copied from the PPU
	case BG1_REGULAR:
	case BG2_REGULAR:
	case BG3_REGULAR: {
		int screenSize;
		bool bpp;
		int characterBaseBlock;
		switch (type) {
		case BG0_REGULAR:
			screenSize = GBA.ppu.bg0ScreenSize;
			bpp = GBA.ppu.bg0Bpp;
			characterBaseBlock = GBA.ppu.bg0CharacterBaseBlock;
			break;
		case BG1_REGULAR:
			screenSize = GBA.ppu.bg1ScreenSize;
			bpp = GBA.ppu.bg1Bpp;
			characterBaseBlock = GBA.ppu.bg1CharacterBaseBlock;
			break;
		case BG2_REGULAR:
			screenSize = GBA.ppu.bg2ScreenSize;
			bpp = GBA.ppu.bg2Bpp;
			characterBaseBlock = GBA.ppu.bg2CharacterBaseBlock;
			break;
		case BG3_REGULAR:
			screenSize = GBA.ppu.bg3ScreenSize;
			bpp = GBA.ppu.bg3Bpp;
			characterBaseBlock = GBA.ppu.bg3CharacterBaseBlock;
			break;
		default: // Get the compiler to shut up
			screenSize = 0;
			bpp = 0;
			characterBaseBlock = 0;
			break;
		}

		int paletteBank = 0;
		bool verticalFlip = false;
		bool horizontolFlip = false;
		int tileIndex = 0;
		int tileRowAddress = 0;

		screenXSize = 256 << (screenSize & 1);
		screenYSize = 256 << ((screenSize >> 1) & 1);

		for (int y = 0; y < screenYSize; y++) {
			for (int x = 0; x < screenXSize; x++) {
				if ((x % 8) == 0) { // Fetch new tile
					int tilemapIndex = (*tilemapIndexLUTDebug[(type * 4) + screenSize])(x, y);

					u16 tilemapEntry = (GBA.ppu.vram[tilemapIndex + 1] << 8) | GBA.ppu.vram[tilemapIndex];
					paletteBank = (tilemapEntry >> 8) & 0xF0;
					verticalFlip = tilemapEntry & 0x0800;
					horizontolFlip = tilemapEntry & 0x0400;
					tileIndex = tilemapEntry & 0x3FF;

					int yMod = verticalFlip ? (7 - (y % 8)) : (y % 8);
					tileRowAddress = (characterBaseBlock * 0x4000) + (tileIndex * (32 + (32 * bpp))) + (yMod * (4 + (bpp * 4)));
				}

				u8 tileData;
				int xMod = horizontolFlip ? (7 - (x % 8)) : (x % 8);
				if (bpp) { // 8 bits per pixel
					tileData = GBA.ppu.vram[tileRowAddress + xMod];
				} else { // 4 bits per pixel
					tileData = GBA.ppu.vram[tileRowAddress + (xMod / 2)];

					if (xMod & 1) {
						tileData >>= 4;
					} else {
						tileData &= 0xF;
					}
				}
				if (tileData != 0) {
					debugBuffer[(y * screenXSize) + x] = convertColor(GBA.ppu.paletteColors[(paletteBank * !bpp) | tileData]);
				} else {
					debugBuffer[(y * screenXSize) + x] = convertColor(GBA.ppu.paletteColors[0]);
				}
			}
		}
		} break;
	case MODE3_BG2:
		for (int line = 0; line < 160; line++) {
			for (int x = 0; x < 240; x++) {
				auto vramIndex = ((line * 240) + x) * 2;
				u16 vramData = (GBA.ppu.vram[vramIndex + 1] << 8) | GBA.ppu.vram[vramIndex];
				buffer[(line * 240) + x] = convertColor(vramData);
			}
		}
		break;
	case MODE4_BG2:
	case MODE4_BG2_FLIPPED:
		for (int line = 0; line < 160; line++) {
			for (int x = 0; x < 240; x++) {
				auto vramIndex = (line * 240) + x + ((type == MODE4_BG2_FLIPPED) * 0xA000);
				u8 vramData = GBA.ppu.vram[vramIndex];
				buffer[(line * 240) + x] = convertColor(GBA.ppu.paletteColors[vramData]);
			}
		}
		break;
	case MODE5_BG2:
	case MODE5_BG2_FLIPPED:
		for (int line = 0; line < 128; line++) {
			for (int x = 0; x < 160; x++) {
				auto vramIndex = (((line * 160) + x) * 2) + ((type == MODE5_BG2_FLIPPED) * 0xA000);
				u16 vramData = (GBA.ppu.vram[vramIndex + 1] << 8) | GBA.ppu.vram[vramIndex];
				buffer[(line * 160) + x] = convertColor(vramData);
			}
		}
		break;
	}
}

bool showLayerView;
void layerViewWindow() {
	ImGui::Begin("Layer View", &showLayerView);

	if (ImGui::BeginCombo("Current Layer", layerInfo[currentlySelectedLayer].name.c_str())) {
		for (int i = 0; i < layerEntriesNum; i++) {
			const bool isSelected = (currentlySelectedLayer == i);

			if (ImGui::Selectable(layerInfo[i].name.c_str(), isSelected))
				currentlySelectedLayer = i;

			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	if (layerInfo[currentlySelectedLayer].enumValue >= MODE3_BG2) {
		ImGui::Text("[%02X.%02X, %02X.%02X]\n[%02X.%02X, %02X.%02X]", GBA.ppu.BG2PA >> 8, GBA.ppu.BG2PA & 0xFF, GBA.ppu.BG2PB >> 8, GBA.ppu.BG2PB & 0xFF, GBA.ppu.BG2PC >> 8, GBA.ppu.BG2PC & 0xFF, GBA.ppu.BG2PD >> 8, GBA.ppu.BG2PD & 0xFF);
	}

	drawDebugLayer(layerInfo[currentlySelectedLayer].enumValue, debugBuffer);
	glBindTexture(GL_TEXTURE_2D, debugTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, screenXSize, screenYSize, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, debugBuffer);
	ImGui::Image((void*)(intptr_t)debugTexture, ImVec2(screenXSize * 2, screenYSize * 2));

	ImGui::End();
}

bool highColor;
int selectedPalette;

bool showTiles;
void tilesWindow() {
	ImGui::Begin("Tiles", &showTiles);

	ImGui::Checkbox("256 Color Mode", &highColor);
	if (!highColor) {
		static char buf[64] = "";
		ImGui::SetNextItemWidth(40);
		ImGui::InputText("Palette (0 to 31)", buf, 64, ImGuiInputTextFlags_CharsDecimal);

		int newPalette = atoi(buf);
		if (newPalette < 32) {
			selectedPalette = newPalette << 4;
		} else {
			sprintf(buf, "%d", selectedPalette >> 4);
		}
	}

	if (highColor) {
		for (int y = 0; y < 256; y++) {
			for (int x = 0; x < (256 / 8); x++) {
				int tileRowAddress = ((x + ((y / 8) * 32)) * 64) + ((y % 8) * 4);

				for (int subX = 0; subX < 8; subX++)
					debugTilesBuffer[(y * 256) + (x * 8) + subX] = convertColor(GBA.ppu.paletteColors[GBA.ppu.vram[tileRowAddress + subX]]);
			}
		}
	} else {
		for (int y = 0; y < 512; y++) {
			for (int x = 0; x < (256 / 8); x++) {
				int tileRowAddress = ((x + ((y / 8) * 32)) * 32) + ((y % 8) * 4);

				debugTilesBuffer[(y * 256) + (x * 8) + 0] = convertColor(GBA.ppu.paletteColors[selectedPalette | (GBA.ppu.vram[tileRowAddress + 0] & 0xF)]);
				debugTilesBuffer[(y * 256) + (x * 8) + 1] = convertColor(GBA.ppu.paletteColors[selectedPalette | (GBA.ppu.vram[tileRowAddress + 0] >> 4)]);
				debugTilesBuffer[(y * 256) + (x * 8) + 2] = convertColor(GBA.ppu.paletteColors[selectedPalette | (GBA.ppu.vram[tileRowAddress + 1] & 0xF)]);
				debugTilesBuffer[(y * 256) + (x * 8) + 3] = convertColor(GBA.ppu.paletteColors[selectedPalette | (GBA.ppu.vram[tileRowAddress + 1] >> 4)]);
				debugTilesBuffer[(y * 256) + (x * 8) + 4] = convertColor(GBA.ppu.paletteColors[selectedPalette | (GBA.ppu.vram[tileRowAddress + 2] & 0xF)]);
				debugTilesBuffer[(y * 256) + (x * 8) + 5] = convertColor(GBA.ppu.paletteColors[selectedPalette | (GBA.ppu.vram[tileRowAddress + 2] >> 4)]);
				debugTilesBuffer[(y * 256) + (x * 8) + 6] = convertColor(GBA.ppu.paletteColors[selectedPalette | (GBA.ppu.vram[tileRowAddress + 3] & 0xF)]);
				debugTilesBuffer[(y * 256) + (x * 8) + 7] = convertColor(GBA.ppu.paletteColors[selectedPalette | (GBA.ppu.vram[tileRowAddress + 3] >> 4)]);
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, debugTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, highColor ? 256 : 512, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, debugTilesBuffer);
	ImGui::Image((void*)(intptr_t)debugTexture, ImVec2(256 * 2, highColor ? 256 : 512 * 2));

	ImGui::End();
}

#define color555to8888(x) \
	(((int)((((x & 0x7C00) >> 10) / (float)31) * 255) << 8) | \
	((int)((((x & 0x03E0) >> 5) / (float)31) * 255) << 16) | \
	((int)(((x & 0x001F) / (float)31) * 255) << 24) | 0xFF)

bool showPalette;
void paletteWindow() {
	static int selectedIndex;
	u16 color = GBA.ppu.paletteColors[selectedIndex];

	ImGui::Begin("Palettes", &showPalette);

	ImGui::Text("Color Index:  %d", selectedIndex);
	ImGui::Text("Memory Location:  0x%07X", 0x5000000 + (selectedIndex * 2));
	ImGui::Text("Color Data:  0x%04X", color);
	ImGui::Text("(r, g, b):  (%d, %d, %d)", color & 0x1F, (color >> 5) & 0x1F, (color >> 10) & 0x1F);

	ImGui::Text("Background");
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));

	for (int y = 0; y < 32; y++) {
		if (y == 16) {
			ImGui::PopStyleVar();
			ImGui::Spacing();
			ImGui::Text("Sprites");
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
		}

		for (int x = 0; x < 16; x++) {
			int index = (y * 16) + x;
			std::string id = "Color " + std::to_string(index);
			u32 color = color555to8888(GBA.ppu.paletteColors[index]);
			ImVec4 colorVec = ImVec4((color >> 24) / 255.0f, ((color >> 16) & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, (color & 0xFF) / 255.0f);

			if (ImGui::ColorButton(id.c_str(), colorVec, (selectedIndex == index) ? 0 : ImGuiColorEditFlags_NoBorder, ImVec2(10, 10)))
				selectedIndex = index;

			if (x != 15)
				ImGui::SameLine();
		}
	}

	ImGui::PopStyleVar();
	ImGui::End();
}