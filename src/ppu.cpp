
#include "ppu.hpp"
#include "imgui.h"
#include "scheduler.hpp"
#include "types.hpp"
#include <array>
#include <cstdio>

#define convertColor(x) (x | 0x8000)

GBAPPU::GBAPPU(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void GBAPPU::reset() {
	// Clear screen and memory
	memset(framebuffer, 0, sizeof(framebuffer));
	memset(vram, 0, sizeof(vram));
	memset(paletteRam, 0, sizeof(paletteRam));
	updateScreen = true;

	currentScanline = 0;

	systemEvents.addEvent(1232, lineStartEvent, this);
	systemEvents.addEvent(960, hBlankEvent, this);
}

void GBAPPU::lineStartEvent(void *object) {
	GBAPPU *ppu = static_cast<GBAPPU *>(object);

	ppu->hBlankFlag = false;
	++ppu->currentScanline;
	if (ppu->currentScanline == 160) { // VBlank
		ppu->updateScreen = true;
		ppu->vBlankFlag = true;
	} else if (ppu->currentScanline == 228) { // Start of frame
		ppu->currentScanline = 0;
		ppu->vBlankFlag = false;
	}

	systemEvents.addEvent(1232, lineStartEvent, object);
}

void GBAPPU::hBlankEvent(void *object) {
	GBAPPU *ppu = static_cast<GBAPPU *>(object);

	if (ppu->currentScanline < 160)
		ppu->drawScanline();
	ppu->hBlankFlag = true;

	systemEvents.addEvent(1232, hBlankEvent, object);
}

template <int bgNum, int size>
int GBAPPU::calculateTilemapIndex(int x, int y) {
	int baseBlock;
	switch (bgNum) {
	case 0: baseBlock = bg0ScreenBaseBlock; break;
	case 1: baseBlock = bg1ScreenBaseBlock; break;
	case 2: baseBlock = bg2ScreenBaseBlock; break;
	case 3: baseBlock = bg3ScreenBaseBlock; break;
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
constexpr std::array<int (GBAPPU::*)(int, int), 16> tilemapIndexLUT = {
	&GBAPPU::calculateTilemapIndex<0, 0>,
	&GBAPPU::calculateTilemapIndex<0, 1>,
	&GBAPPU::calculateTilemapIndex<0, 2>,
	&GBAPPU::calculateTilemapIndex<0, 3>,
	&GBAPPU::calculateTilemapIndex<1, 0>,
	&GBAPPU::calculateTilemapIndex<1, 1>,
	&GBAPPU::calculateTilemapIndex<1, 2>,
	&GBAPPU::calculateTilemapIndex<1, 3>,
	&GBAPPU::calculateTilemapIndex<2, 0>,
	&GBAPPU::calculateTilemapIndex<2, 1>,
	&GBAPPU::calculateTilemapIndex<2, 2>,
	&GBAPPU::calculateTilemapIndex<2, 3>,
	&GBAPPU::calculateTilemapIndex<3, 0>,
	&GBAPPU::calculateTilemapIndex<3, 1>,
	&GBAPPU::calculateTilemapIndex<3, 2>,
	&GBAPPU::calculateTilemapIndex<3, 3>
};

template <int bgNum>
void GBAPPU::drawBg() {
	int xOffset;
	int yOffset;
	int screenSize;
	bool bpp;
	int characterBaseBlock;
	switch (bgNum) {
	case 0:
		xOffset = bg0XOffset;
		yOffset = bg0YOffset;
		screenSize = bg0ScreenSize;
		bpp = bg0Bpp;
		characterBaseBlock = bg0CharacterBaseBlock;
		break;
	case 1:
		xOffset = bg1XOffset;
		yOffset = bg1YOffset;
		screenSize = bg1ScreenSize;
		bpp = bg1Bpp;
		characterBaseBlock = bg1CharacterBaseBlock;
		break;
	case 2:
		xOffset = bg2XOffset;
		yOffset = bg2YOffset;
		screenSize = bg2ScreenSize;
		bpp = bg2Bpp;
		characterBaseBlock = bg2CharacterBaseBlock;
		break;
	case 3:
		xOffset = bg3XOffset;
		yOffset = bg3YOffset;
		screenSize = bg3ScreenSize;
		bpp = bg3Bpp;
		characterBaseBlock = bg3CharacterBaseBlock;
		break;
	}

	int paletteBank = 0;
	bool verticalFlip = false;
	bool horizontolFlip = false;
	int tileIndex = 0;
	int tileRowAddress = 0;

	int x = xOffset;
	int y = currentScanline + yOffset;
	for (int i = 0; i < 240; i++) {
		if (((x % 8) == 0) || (i == 0)) { // Fetch new tile
			int tilemapIndex = (this->*tilemapIndexLUT[(bgNum * 4) + screenSize])(x, y);

			u16 tilemapEntry = (vram[tilemapIndex + 1] << 8) | vram[tilemapIndex];
			paletteBank = (tilemapEntry >> 8) & 0xF0;
			verticalFlip = tilemapEntry & 0x0800;
			horizontolFlip = tilemapEntry & 0x0400;
			tileIndex = tilemapEntry & 0x3FF;

			int yMod = verticalFlip ? (7 - (y % 8)) : (y % 8);
			tileRowAddress = (characterBaseBlock * 0x4000) + (tileIndex * (32 + (32 * bpp))) + (yMod * (4 + (bpp * 4)));
		}
		if (tileRowAddress >= 0x10000)
			break;

		u8 tileData;
		int xMod = horizontolFlip ? (7 - (x % 8)) : (x % 8);
		if (bpp) { // 8 bits per pixel
			tileData = vram[tileRowAddress + xMod];
		} else { // 4 bits per pixel
			tileData = vram[tileRowAddress + (xMod / 2)];

			if (xMod & 1) {
				tileData >>= 4;
			} else {
				tileData &= 0xF;
			}
		}
		if (tileData != 0) {
			framebuffer[currentScanline][i] = convertColor(paletteColors[(paletteBank * !bpp) | tileData]);
		}

		++x;
	}
}

void GBAPPU::drawScanline() {
	switch (bgMode) {
	case 0:
		for (int i = 0; i < 240; i++)
			framebuffer[currentScanline][i] = convertColor(paletteColors[0]);

		for (int layer = 0; layer < 4; layer++) {
			if ((bg0Priority == layer) && screenDisplayBg0) drawBg<0>();
			if ((bg1Priority == layer) && screenDisplayBg1) drawBg<1>();
			if ((bg2Priority == layer) && screenDisplayBg2) drawBg<2>();
			if ((bg3Priority == layer) && screenDisplayBg3) drawBg<3>();
		}

		/*for (int i = 0; i < 8; i++) { // TODO: reverse bit shifting
			int tileRowAddress = (bg0CharacterBaseBlock * 0x4000) + ((i + ((currentScanline / 8) * 8)) * 32) + ((currentScanline % 8) * 4);
			framebuffer[currentScanline][(i * 8) + 0] = convertColor(paletteColors[0x30 | vram[tileRowAddress + 0] >> 4]);
			framebuffer[currentScanline][(i * 8) + 1] = convertColor(paletteColors[0x30 | vram[tileRowAddress + 0] & 0xF]);
			framebuffer[currentScanline][(i * 8) + 2] = convertColor(paletteColors[0x30 | vram[tileRowAddress + 1] >> 4]);
			framebuffer[currentScanline][(i * 8) + 3] = convertColor(paletteColors[0x30 | vram[tileRowAddress + 1] & 0xF]);
			framebuffer[currentScanline][(i * 8) + 4] = convertColor(paletteColors[0x30 | vram[tileRowAddress + 2] >> 4]);
			framebuffer[currentScanline][(i * 8) + 5] = convertColor(paletteColors[0x30 | vram[tileRowAddress + 2] & 0xF]);
			framebuffer[currentScanline][(i * 8) + 6] = convertColor(paletteColors[0x30 | vram[tileRowAddress + 3] >> 4]);
			framebuffer[currentScanline][(i * 8) + 7] = convertColor(paletteColors[0x30 | vram[tileRowAddress + 3] & 0xF]);
		}*/
		break;
	case 1:
		for (int i = 0; i < 240; i++)
			framebuffer[currentScanline][i] = convertColor(paletteColors[0]);

		for (int layer = 0; layer < 4; layer++) {
			if ((bg0Priority == layer) && screenDisplayBg0) drawBg<0>();
			if ((bg1Priority == layer) && screenDisplayBg1) drawBg<1>();
		}
		break;
	case 3:
		for (int i = 0; i < 240; i++) {
			auto vramIndex = ((currentScanline * 240) + i) * 2;
			u16 vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
			framebuffer[currentScanline][i] = convertColor(vramData);
		}
		break;
	case 4:
		for (int i = 0; i < 240; i++) {
			auto vramIndex = ((currentScanline * 240) + i) + (displayFrameSelect * 0xA000);
			u8 vramData = vram[vramIndex];
			framebuffer[currentScanline][i] = convertColor(paletteColors[vramData]);
		}
		break;
	case 5:
		for (int x = 0; x < 240; x++) {
			if ((x < 160) && (currentScanline < 128)) {
				auto vramIndex = (((currentScanline * 160) + x) * 2) + (displayFrameSelect * 0xA000);
				u16 vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
				framebuffer[currentScanline][x] = convertColor(vramData);
			} else {
				framebuffer[currentScanline][x] = convertColor(paletteColors[0]);
			}
		}
		break;
	default:
		for (int i = 0; i < 240; i++)
			framebuffer[currentScanline][i] = 1; // Draw black
		break;
	}
}

u8 GBAPPU::readIO(u32 address) {
	switch (address) {
	case 0x4000000:
		return (u8)DISPCNT;
	case 0x4000001:
		return (u8)(DISPCNT >> 8);
	case 0x4000004:
		return (u8)DISPSTAT;
	case 0x4000005:
		return (u8)(DISPSTAT >> 8);
	case 0x4000006:
		return (u8)VCOUNT;
	case 0x4000007:
		return (u8)(VCOUNT >> 8);
	case 0x4000008:
		return (u8)BG0CNT;
	case 0x4000009:
		return (u8)(BG0CNT >> 8);
	case 0x400000A:
		return (u8)BG1CNT;
	case 0x400000B:
		return (u8)(BG1CNT >> 8);
	case 0x400000C:
		return (u8)BG2CNT;
	case 0x400000D:
		return (u8)(BG2CNT >> 8);
	case 0x400000E:
		return (u8)BG3CNT;
	case 0x400000F:
		return (u8)(BG3CNT >> 8);
	default:
		return 0;
	}
}

void GBAPPU::writeIO(u32 address, u8 value) {
	switch (address) {
	case 0x4000000:
		DISPCNT = (DISPCNT & 0xFF00) | value;
		break;
	case 0x4000001:
		DISPCNT = (DISPCNT & 0x00FF) | (value << 8);
		break;
	case 0x4000004:
		DISPSTAT = (DISPSTAT & 0xFF00) | (value & 0x38);
		break;
	case 0x4000005:
		DISPSTAT = (DISPSTAT & 0x00FF) | (value << 8);
		break;
	case 0x4000008:
		BG0CNT = (BG0CNT & 0xFF00) | value;
		break;
	case 0x4000009:
		BG0CNT = (BG0CNT & 0x00FF) | (value << 8);
		break;
	case 0x400000A:
		BG1CNT = (BG1CNT & 0xFF00) | value;
		break;
	case 0x400000B:
		BG1CNT = (BG1CNT & 0x00FF) | (value << 8);
		break;
	case 0x400000C:
		BG2CNT = (BG2CNT & 0xFF00) | value;
		break;
	case 0x400000d:
		BG2CNT = (BG2CNT & 0x00FF) | (value << 8);
		break;
	case 0x400000E:
		BG3CNT = (BG3CNT & 0xFF00) | value;
		break;
	case 0x400000F:
		BG3CNT = (BG3CNT & 0x00FF) | (value << 8);
		break;
	case 0x4000010:
		bg0XOffset = (bg0XOffset & 0x0100) | value;
		break;
	case 0x4000011:
		bg0XOffset = (bg0XOffset & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x4000012:
		bg0YOffset = (bg0YOffset & 0x0100) | value;
		break;
	case 0x4000013:
		bg0YOffset = (bg0YOffset & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x4000014:
		bg1XOffset = (bg1XOffset & 0x0100) | value;
		break;
	case 0x4000015:
		bg1XOffset = (bg1XOffset & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x4000016:
		bg1YOffset = (bg1YOffset & 0x0100) | value;
		break;
	case 0x4000017:
		bg1YOffset = (bg1YOffset & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x4000018:
		bg2XOffset = (bg2XOffset & 0x0100) | value;
		break;
	case 0x4000019:
		bg2XOffset = (bg2XOffset & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x400001A:
		bg2YOffset = (bg2YOffset & 0x0100) | value;
		break;
	case 0x400001B:
		bg2YOffset = (bg2YOffset & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x400001C:
		bg3XOffset = (bg3XOffset & 0x0100) | value;
		break;
	case 0x400001D:
		bg3XOffset = (bg3XOffset & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x400001E:
		bg3YOffset = (bg3YOffset & 0x0100) | value;
		break;
	case 0x400001F:
		bg3YOffset = (bg3YOffset & 0x00FF) | ((value & 1) << 8);
		break;
	}
}