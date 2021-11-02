
#include "ppu.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include <array>
#include <cstdio>

#define convertColor(x) (x | 0x8000)

GBAPPU::GBAPPU(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void GBAPPU::reset() {
	// Clear screen and memory
	memset(framebuffer, 0, sizeof(framebuffer));
	updateScreen = true;
	memset(paletteRam, 0, sizeof(paletteRam));
	memset(vram, 0, sizeof(vram));
	memset(oam, 0, sizeof(oam));

	currentScanline = 0;

	systemEvents.addEvent(1232, lineStartEvent, this);
	systemEvents.addEvent(960, hBlankEvent, this);
}

void GBAPPU::lineStartEvent(void *object) {
	static_cast<GBAPPU *>(object)->lineStart();
}

void GBAPPU::lineStart() {
	hBlankFlag = false;
	++currentScanline;
	if (currentScanline == 160) { // VBlank
		updateScreen = true;
		vBlankFlag = true;

		if (vBlankIrqEnable)
			bus.cpu.requestInterrupt(GBACPU::IRQ_VBLANK);
		
		bus.dma.onVBlank();
	} else if (currentScanline == 228) { // Start of frame
		currentScanline = 0;
		vBlankFlag = false;
	}

	if (vCountSetting == currentScanline) {
		vCounterFlag = true;

		if (vCounterIrqEnable)
			bus.cpu.requestInterrupt(GBACPU::IRQ_VCOUNT);
	} else {
		vCounterFlag = false;
	}

	systemEvents.addEvent(1232, lineStartEvent, this);
}

void GBAPPU::hBlankEvent(void *object) {
	static_cast<GBAPPU *>(object)->hBlank();
}

void GBAPPU::hBlank() {
	if (currentScanline < 160)
		drawScanline();

	hBlankFlag = true;
	if (hBlankIrqEnable)
		bus.cpu.requestInterrupt(GBACPU::IRQ_HBLANK);

	if (!vBlankFlag)
		bus.dma.onHBlank();

	systemEvents.addEvent(1232, hBlankEvent, this);
}

static const int objSizeArray[3][4][2] = {
	{{8, 8}, {16, 16}, {32, 32}, {64, 64}},
	{{16, 8}, {32, 8}, {32, 16}, {64, 32}},
	{{8, 16}, {8, 32}, {16, 32}, {32, 64}}
};

void GBAPPU::drawObjects() {
	int tileRowAddress = 0;
	bool win0VertFits = (win0Top > win0Bottom) ? ((currentScanline >= win0Top) || (currentScanline < win0Bottom)) : ((currentScanline >= win0Top) && (currentScanline < win0Bottom));
	bool win1VertFits = (win1Top > win1Bottom) ? ((currentScanline >= win1Top) || (currentScanline < win1Bottom)) : ((currentScanline >= win1Top) && (currentScanline < win1Bottom));

	for (int priority = 3; priority >= 0; priority--) {
		for (int i = 0; i < 240; i++)
			lineBuffer[4 + priority][i].priority = -1;

		for (int objNo = 127; objNo >= 0; objNo--) {
			if ((objects[objNo].priority == priority) && (objects[objNo].objMode != 2)) {
				Object *obj = &objects[objNo];

				int xSize = objSizeArray[obj->shape][obj->size][0];
				int ySize = objSizeArray[obj->shape][obj->size][1];

				unsigned int x = obj->objX;
				u8 y = currentScanline - obj->objY;
				int yMod = obj->verticalFlip ? (7 - (y % 8)) : (y % 8);

				if ((obj->objY + ySize) > 255) {
					if ((currentScanline < obj->objY) && (currentScanline >= (u8)(obj->objY + ySize)))
						continue;
				} else {
					if ((currentScanline < obj->objY) || (currentScanline >= (obj->objY + ySize)))
						continue;
				}

				for (int relX = 0; relX < xSize; relX++) {
					if (x < 240) {
						if (((relX % 8) == 0) || (x == 0)) // Fetch new tile
							tileRowAddress = 0x10000 + ((obj->tileIndex & ~(1 * obj->bpp)) * 32) + (((((obj->verticalFlip ? (ySize - 1 - y) : y) / 8) * (objMappingDimension ? (xSize / 8) : 32)) + ((obj->horizontolFlip ? (xSize - 1 - relX) : relX) / 8)) * (32 << obj->bpp)) + (yMod * (4 << obj->bpp));
						if (((tileRowAddress <= 0x14000) && (bgMode >= 3)) || (tileRowAddress >= 0x18000))
							break;

						u8 tileData;
						int xMod = obj->horizontolFlip ? (7 - (relX % 8)) : (relX % 8);
						if (obj->bpp) { // 8 bits per pixel
							tileData = vram[tileRowAddress + xMod];
						} else { // 4 bits per pixel
							tileData = vram[tileRowAddress + (xMod / 2)];

							if (xMod & 1) {
								tileData >>= 4;
							} else {
								tileData &= 0xF;
							}
						}

						Pixel *pix = &lineBuffer[4 + priority][x];
						pix->inWin0 = false;
						pix->inWin1 = false;
						pix->inWinOut = false;

						if (window0DisplayFlag)
							if ((x >= win0Left) && (x < win0Right) && win0VertFits)
								pix->inWin0 = true;
						if (window1DisplayFlag)
							if ((x >= win1Left) && (x < win1Right) && win1VertFits)
								pix->inWin1 = true;
						pix->inWinOut = !(pix->inWin0 || pix->inWin1);

						if (tileData != 0) {
							pix->priority = priority;
							pix->color = convertColor(paletteColors[0x100 | ((obj->palette << 4) * !obj->bpp) | tileData]);
						}
					}

					x = (x + 1) & 0x1FF;
				}
			}
		}
	}
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

	bool win0VertFits = (win0Top > win0Bottom) ? ((currentScanline >= win0Top) || (currentScanline < win0Bottom)) : ((currentScanline >= win0Top) && (currentScanline < win0Bottom));
	bool win1VertFits = (win1Top > win1Bottom) ? ((currentScanline >= win1Top) || (currentScanline < win1Bottom)) : ((currentScanline >= win1Top) && (currentScanline < win1Bottom));
	for (int i = 0; i < 240; i++) {
		if (((x % 8) == 0) || (i == 0)) { // Fetch new tile
			int tilemapIndex = (this->*tilemapIndexLUT[(bgNum * 4) + screenSize])(x, y);

			u16 tilemapEntry = (vram[tilemapIndex + 1] << 8) | vram[tilemapIndex];
			paletteBank = (tilemapEntry >> 8) & 0xF0;
			verticalFlip = tilemapEntry & 0x0800;
			horizontolFlip = tilemapEntry & 0x0400;
			tileIndex = tilemapEntry & 0x3FF;

			int yMod = verticalFlip ? (7 - (y % 8)) : (y % 8);
			tileRowAddress = (characterBaseBlock * 0x4000) + (tileIndex * (32 << bpp)) + (yMod * (4 << bpp));
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

		Pixel *pix = &lineBuffer[bgNum][i];
		pix->inWin0 = false;
		pix->inWin1 = false;
		pix->inWinOut = false;

		if (window0DisplayFlag)
			if ((i >= win0Left) && (i < win0Right) && win0VertFits)
				pix->inWin0 = true;
		if (window1DisplayFlag)
			if ((i >= win1Left) && (i < win1Right) && win1VertFits)
				pix->inWin1 = true;
		pix->inWinOut = !(pix->inWin0 || pix->inWin1);

		pix->priority = (tileData == 0) ? -1 : bgNum;
		pix->color = convertColor(paletteColors[(paletteBank * !bpp) | tileData]);

		++x;
	}
}

void GBAPPU::drawScanline() {
	switch (bgMode) {
	case 0:
		if (screenDisplayBg0) drawBg<0>();
		if (screenDisplayBg1) drawBg<1>();
		if (screenDisplayBg2) drawBg<2>();
		if (screenDisplayBg3) drawBg<3>();
		if (screenDisplayObj) drawObjects();

		for (int i = 0; i < 240; i++) {
			mergedBuffer[i].color = convertColor(paletteColors[0]);

			// Window
			if (window0DisplayFlag || window1DisplayFlag) {
				for (int layer = 3; layer >= 0; layer--) {
					if (lineBuffer[3][i].inWinOut && winOutBg3Enable && (bg3Priority == layer) && screenDisplayBg3 && (lineBuffer[3][i].priority != -1)) mergedBuffer[i] = lineBuffer[3][i];
					if (lineBuffer[2][i].inWinOut && winOutBg2Enable && (bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
					if (lineBuffer[1][i].inWinOut && winOutBg1Enable && (bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
					if (lineBuffer[0][i].inWinOut && winOutBg0Enable && (bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
					if (lineBuffer[4 + layer][i].inWinOut && winOutObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
				}

				if (window1DisplayFlag) {
					for (int layer = 3; layer >= 0; layer--) {
						if (lineBuffer[3][i].inWin1 && !lineBuffer[3][i].inWin0 && win1Bg3Enable && (bg3Priority == layer) && screenDisplayBg3 && (lineBuffer[3][i].priority != -1)) mergedBuffer[i] = lineBuffer[3][i];
						if (lineBuffer[2][i].inWin1 && !lineBuffer[2][i].inWin0 && win1Bg2Enable && (bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
						if (lineBuffer[1][i].inWin1 && !lineBuffer[1][i].inWin0 && win1Bg1Enable && (bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
						if (lineBuffer[0][i].inWin1 && !lineBuffer[0][i].inWin0 && win1Bg0Enable && (bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
						if (lineBuffer[4 + layer][i].inWin1 && !lineBuffer[4 + layer][i].inWin0 && win1ObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
					}
				}

				if (window0DisplayFlag) {
					for (int layer = 3; layer >= 0; layer--) {
						if (lineBuffer[3][i].inWin0 && win0Bg3Enable && (bg3Priority == layer) && screenDisplayBg3 && (lineBuffer[3][i].priority != -1)) mergedBuffer[i] = lineBuffer[3][i];
						if (lineBuffer[2][i].inWin0 && win0Bg2Enable && (bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
						if (lineBuffer[1][i].inWin0 && win0Bg1Enable && (bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
						if (lineBuffer[0][i].inWin0 && win0Bg0Enable && (bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
						if (lineBuffer[4 + layer][i].inWin0 && win0ObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
					}
				}
			} else {
				for (int layer = 3; layer >= 0; layer--) {
					if ((bg3Priority == layer) && screenDisplayBg3 && (lineBuffer[3][i].priority != -1)) mergedBuffer[i] = lineBuffer[3][i];
					if ((bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
					if ((bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
					if ((bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
					if (screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
				}
			}

			framebuffer[currentScanline][i] = mergedBuffer[i].color;
		}
		break;
	case 1:
		if (screenDisplayBg0) drawBg<0>();
		if (screenDisplayBg1) drawBg<1>();
		if (screenDisplayObj) drawObjects();

		for (int i = 0; i < 240; i++) {
			mergedBuffer[i].color = convertColor(paletteColors[0]);

			// Window
			if (window0DisplayFlag || window1DisplayFlag) {
				for (int layer = 3; layer >= 0; layer--) {
					if (lineBuffer[1][i].inWinOut && winOutBg1Enable && (bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
					if (lineBuffer[0][i].inWinOut && winOutBg0Enable && (bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
					if (lineBuffer[4 + layer][i].inWinOut && winOutObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
				}

				if (window1DisplayFlag) {
					for (int layer = 3; layer >= 0; layer--) {
						if (lineBuffer[1][i].inWin1 && !lineBuffer[1][i].inWin0 && win1Bg1Enable && (bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
						if (lineBuffer[0][i].inWin1 && !lineBuffer[0][i].inWin0 && win1Bg0Enable && (bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
						if (lineBuffer[4 + layer][i].inWin1 && !lineBuffer[4 + layer][i].inWin0 && win1ObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
					}
				}

				if (window0DisplayFlag) {
					for (int layer = 3; layer >= 0; layer--) {
						if (lineBuffer[1][i].inWin0 && win0Bg1Enable && (bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
						if (lineBuffer[0][i].inWin0 && win0Bg0Enable && (bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
						if (lineBuffer[4 + layer][i].inWin0 && win0ObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
					}
				}
			} else {
				for (int layer = 3; layer >= 0; layer--) {
					if ((bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
					if ((bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
					if (screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
				}
			}

			framebuffer[currentScanline][i] = mergedBuffer[i].color;
		}
		break;
	case 3:
		drawObjects();
		for (int i = 0; i < 240; i++) {
			auto vramIndex = ((currentScanline * 240) + i) * 2;
			u16 vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
			framebuffer[currentScanline][i] = convertColor(vramData);

			for (int layer = 3; layer >= 0; layer--) {
				if (screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1))
					framebuffer[currentScanline][i] = lineBuffer[4 + layer][i].color;
			}
		}
		break;
	case 4:
		drawObjects();
		for (int i = 0; i < 240; i++) {
			auto vramIndex = ((currentScanline * 240) + i) + (displayFrameSelect * 0xA000);
			u8 vramData = vram[vramIndex];
			framebuffer[currentScanline][i] = convertColor(paletteColors[vramData]);

			for (int layer = 3; layer >= 0; layer--) {
				if (screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1))
					framebuffer[currentScanline][i] = lineBuffer[4 + layer][i].color;
			}
		}
		break;
	case 5:
		drawObjects();
		for (int x = 0; x < 240; x++) {
			if ((x < 160) && (currentScanline < 128)) {
				auto vramIndex = (((currentScanline * 160) + x) * 2) + (displayFrameSelect * 0xA000);
				u16 vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
				framebuffer[currentScanline][x] = convertColor(vramData);
			} else {
				framebuffer[currentScanline][x] = convertColor(paletteColors[0]);
			}

			for (int layer = 3; layer >= 0; layer--) {
				if (screenDisplayObj && (lineBuffer[4 + layer][x].priority != -1))
					framebuffer[currentScanline][x] = lineBuffer[4 + layer][x].color;
			}
		}
		break;
	default:
		for (int i = 0; i < 240; i++)
			framebuffer[currentScanline][i] = 0x8000; // Draw black
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
	case 0x4000048:
		return (u8)WININ;
	case 0x4000049:
		return (u8)(WININ >> 8);
	case 0x400004A:
		return (u8)WINOUT;
	case 0x400004B:
		return (u8)(WINOUT >> 8);
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
	case 0x4000040:
		WIN0H = (WIN0H & 0xFF00) | value;
		break;
	case 0x4000041:
		WIN0H = (WIN0H & 0x00FF) | (value << 8);
		break;
	case 0x4000042:
		WIN1H = (WIN1H & 0xFF00) | value;
		break;
	case 0x4000043:
		WIN1H = (WIN1H & 0x00FF) | (value << 8);
		break;
	case 0x4000044:
		WIN0V = (WIN0V & 0xFF00) | value;
		break;
	case 0x4000045:
		WIN0V = (WIN0V & 0x00FF) | (value << 8);
		break;
	case 0x4000046:
		WIN1V = (WIN1V & 0xFF00) | value;
		break;
	case 0x4000047:
		WIN1V = (WIN1V & 0x00FF) | (value << 8);
		break;
	case 0x4000048:
		WININ = (WININ & 0x3F00) | (value & 0x3F);
		break;
	case 0x4000049:
		WININ = (WININ & 0x003F) | ((value & 0x3F) << 8);
		break;
	case 0x400004A:
		WINOUT = (WINOUT & 0x3F00) | (value & 0x3F);
		break;
	case 0x400004B:
		WINOUT = (WINOUT & 0x003F) | ((value & 0x3F) << 8);
		break;
	}
}