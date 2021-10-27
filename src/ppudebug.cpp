
#include "ppudebug.hpp"
#include "SDL_opengl.h"
#include "imgui.h"

#define convertColor(x) (x | 0x8000)

GLuint debugTexture;
u16 debugBuffer[240 * 160];
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
	{MODE3_BG2, "Mode 3, Background 2", 240, 160},
	{MODE4_BG2, "Mode 4, Background 2", 240, 160},
	{MODE4_BG2_FLIPPED, "Mode 4, Background 2, Flipped", 240, 160},
	{MODE5_BG2, "Mode 5, Background 2", 160, 128},
	{MODE5_BG2_FLIPPED, "Mode 5, Background 2, Flipped", 160, 128}
};
const int layerEntriesNum = sizeof(layerInfo)/sizeof(layerInfoEntry);
int currentlySelectedLayer = 0;

void drawDebugLayer(bgLayer type, u16 *buffer) {
	switch (type) {
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
		for (int line = 0; line < 160; line++) {
			for (int x = 0; x < 240; x++) {
				auto vramIndex = (line * 240) + x;
				u8 vramData = GBA.ppu.vram[vramIndex];
				buffer[(line * 240) + x] = convertColor(GBA.ppu.paletteColors[vramData]);
			}
		}
		break;
	case MODE4_BG2_FLIPPED:
		for (int line = 0; line < 160; line++) {
			for (int x = 0; x < 240; x++) {
				auto vramIndex = (line * 240) + x + 0xA000;
				u8 vramData = GBA.ppu.vram[vramIndex];
				buffer[(line * 240) + x] = convertColor(GBA.ppu.paletteColors[vramData]);
			}
		}
		break;
	case MODE5_BG2:
		for (int line = 0; line < 128; line++) {
			for (int x = 0; x < 160; x++) {
				auto vramIndex = ((line * 160) + x) * 2;
				u16 vramData = (GBA.ppu.vram[vramIndex + 1] << 8) | GBA.ppu.vram[vramIndex];
				buffer[(line * 160) + x] = convertColor(vramData);
			}
		}
		break;
	case MODE5_BG2_FLIPPED:
		for (int line = 0; line < 128; line++) {
			for (int x = 0; x < 160; x++) {
				auto vramIndex = (((line * 160) + x) * 2) + 0xA000;
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

	drawDebugLayer(layerInfo[currentlySelectedLayer].enumValue, debugBuffer);
	glBindTexture(GL_TEXTURE_2D, debugTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, layerInfo[currentlySelectedLayer].xSize, layerInfo[currentlySelectedLayer].ySize, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, debugBuffer);
	ImGui::Image((void*)(intptr_t)debugTexture, ImVec2(layerInfo[currentlySelectedLayer].xSize * 2, layerInfo[currentlySelectedLayer].ySize * 2));

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

	ImGui::Begin("Palettes", &showPalette);

	ImGui::Text("Color Index:  %d", selectedIndex);
	ImGui::Text("Memory Location:  0x%07X", 0x5000000 + (selectedIndex * 2));
	ImGui::Text("Color Data:  0x%04X", GBA.ppu.paletteColors[selectedIndex]);

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