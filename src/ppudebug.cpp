
#include "ppudebug.hpp"

#define convertColor(x) ((x << 1) | 1)

u16 debugBuffer[240 * 160];

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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, layerInfo[currentlySelectedLayer].xSize, layerInfo[currentlySelectedLayer].ySize, 0, GL_BGRA, GL_UNSIGNED_SHORT_5_5_5_1, debugBuffer);
	ImGui::Image((void*)(intptr_t)debugTexture, ImVec2(layerInfo[currentlySelectedLayer].xSize * 2, layerInfo[currentlySelectedLayer].ySize * 2));

	ImGui::End();
}