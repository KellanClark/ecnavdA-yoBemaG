
#include "ppu.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include "types.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

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

	win0VertFits = win1VertFits = false;
	internalBG2X = internalBG2Y = internalBG3X = internalBG3Y = 0;

	DISPCNT = 0;
	DISPSTAT = 0;
	VCOUNT = 0;
	BG0CNT = BG1CNT = BG2CNT = BG3CNT = 0;
	BG0HOFS = BG0VOFS = 0;
	BG1HOFS = BG1VOFS = 0;
	BG2HOFS = BG2VOFS = 0;
	BG3HOFS = BG3VOFS = 0;
	BG2PA = BG2PB = BG2PC = BG2PD = BG2X = BG2Y = 0;
	BG3PA = BG3PB = BG3PC = BG3PD = BG3X = BG3Y = 0;

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

		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 255;
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 255;
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 255;
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 255;
	}

	if (vCountSetting == currentScanline) {
		vCounterFlag = true;

		if (vCounterIrqEnable)
			bus.cpu.requestInterrupt(GBACPU::IRQ_VCOUNT);
	} else {
		vCounterFlag = false;
	}

	if (currentScanline == win0Top)
		win0VertFits = true;
	if (currentScanline == win0Bottom)
		win0VertFits = false;
	if (currentScanline == win1Top)
		win1VertFits = true;
	if (currentScanline == win1Bottom)
		win1VertFits = false;

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
	int tileDataAddress = 0;
	u8 tileData = 0;

	for (int priority = 3; priority >= 0; priority--) {
		for (int i = 0; i < 240; i++)
			lineBuffer[4 + priority][i].priority = -1;

		for (int objNo = 127; objNo >= 0; objNo--) {
			if ((objects[objNo].priority == priority) && (objects[objNo].objMode != 2)) {
				Object *obj = &objects[objNo];
				ObjectMatrix mat = objectMatrices[obj->affineIndex];

				int xSize = objSizeArray[obj->shape][obj->size][0];
				int ySize = objSizeArray[obj->shape][obj->size][1];

				unsigned int x = obj->objX;
				u8 y = currentScanline - obj->objY;
				int yMod = obj->verticalFlip ? (7 - (y % 8)) : (y % 8);

				float affX = 0;
				float affY = 0;
				if (obj->objMode == 1) { // Affine
					affX = (((float)((i16)mat.pb) / 255) * (y - ((float)(ySize - 1) / 2))) + (((float)((i16)mat.pa) / 255) * ((float)(xSize - 1) / -2)) + ((float)(xSize - 1) / 2);
					affY = (((float)((i16)mat.pd) / 255) * (y - ((float)(ySize - 1) / 2))) + (((float)((i16)mat.pc) / 255) * ((float)(xSize - 1) / -2)) + ((float)(ySize - 1) / 2);
				} else if (obj->objMode == 3) { // Affine double size
					affX = (((float)((i16)mat.pb) / 255) * (y - (float)(ySize - 1))) + (((float)((i16)mat.pa) / 255) * ((float)(xSize - 1) * -1)) + ((float)(xSize - 1) / 2);
					affY = (((float)((i16)mat.pd) / 255) * (y - (float)(ySize - 1))) + (((float)((i16)mat.pc) / 255) * ((float)(xSize - 1) * -1)) + ((float)(ySize - 1) / 2);

					ySize <<= 1;
				}

				if ((obj->objY + ySize) > 255) { // Decide if object is on this scanline
					if ((currentScanline < obj->objY) && (currentScanline >= (u8)(obj->objY + ySize)))
						continue;
				} else {
					if ((currentScanline < obj->objY) || (currentScanline >= (obj->objY + ySize)))
						continue;
				}

				if (obj->objMode == 3)
					ySize >>= 1;

				for (int relX = 0; relX < (xSize << (obj->objMode == 3)); relX++) {
					if (x < 240) {
						if ((obj->objMode == 1) || (obj->objMode == 3)) {
							if ((affX >= 0) && (affX < xSize) && (affY >= 0) && (affY < ySize)) {
								tileDataAddress = 0x10000 + ((obj->tileIndex & ~(1 * obj->bpp)) * 32) + (((((int)affY / 8) * (objMappingDimension ? (xSize / 8) : 32)) + ((int)affX / 8)) * (32 << obj->bpp)) + (((int)affY % 8) * (4 << obj->bpp)) + (((int)affX % 8) / (2 >> obj->bpp));

								tileData = vram[tileDataAddress];
								if ((int)affX & 1) {
									tileData >>= 4;
								} else {
									tileData &= 0xF;
								}
							} else {
								tileData = 0;
							}

							affX += (float)((i16)mat.pa) / 255;
							affY += (float)((i16)mat.pc) / 255;
						} else {
							if (((relX % 8) == 0) || (x == 0)) { // Fetch new tile
								tileRowAddress = 0x10000 + ((obj->tileIndex & ~(1 * obj->bpp)) * 32) + (((((obj->verticalFlip ? (ySize - 1 - y) : y) / 8) * (objMappingDimension ? (xSize / 8) : 32)) + ((obj->horizontolFlip ? (xSize - 1 - relX) : relX) / 8)) * (32 << obj->bpp)) + (yMod * (4 << obj->bpp));
							}
							if (((tileRowAddress <= 0x14000) && (bgMode >= 3)) || (tileRowAddress >= 0x18000))
								break;

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
		xOffset = BG0HOFS;
		yOffset = BG0VOFS;
		screenSize = bg0ScreenSize;
		bpp = bg0Bpp;
		characterBaseBlock = bg0CharacterBaseBlock;
		break;
	case 1:
		xOffset = BG1HOFS;
		yOffset = BG1VOFS;
		screenSize = bg1ScreenSize;
		bpp = bg1Bpp;
		characterBaseBlock = bg1CharacterBaseBlock;
		break;
	case 2:
		xOffset = BG2HOFS;
		yOffset = BG2VOFS;
		screenSize = bg2ScreenSize;
		bpp = bg2Bpp;
		characterBaseBlock = bg2CharacterBaseBlock;
		break;
	case 3:
		xOffset = BG3HOFS;
		yOffset = BG3VOFS;
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

	bool win0HorzFits = win0Right < win0Left;
	bool win1HorzFits = win1Right < win1Left;

	for (int i = 0; i < 240; i++) {
		if (win0Left == i)
			win0HorzFits = true;
		if (win0Right == i)
			win0HorzFits = false;
		if (win1Left == i)
			win1HorzFits = true;
		if (win1Right == i)
			win1HorzFits = false;

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
			if (win0HorzFits && win0VertFits)
				pix->inWin0 = true;
		if (window1DisplayFlag)
			if (win1HorzFits && win1VertFits)
				pix->inWin1 = true;
		pix->inWinOut = !(pix->inWin0 || pix->inWin1);

		pix->priority = (tileData == 0) ? -1 : bgNum;
		pix->color = convertColor(paletteColors[(paletteBank * !bpp) | tileData]);

		++x;
	}
}

template <int bgNum>
void GBAPPU::drawBgAff() {
	int characterBaseBlock;
	int screenBaseBlock;
	bool wrapping;
	int screenSize;
	float affX;
	float affY;
	float pa;
	float pc;
	if (bgNum == 2) {
		characterBaseBlock = bg2CharacterBaseBlock;
		screenBaseBlock = bg2ScreenBaseBlock;
		wrapping = bg2Wrapping;
		screenSize = 128 << bg2ScreenSize;
		affX = internalBG2X;
		affY = internalBG2Y;
		pa = (float)((i16)BG2PA) / 255;
		pc = (float)((i16)BG2PC) / 255;
	} else if (bgNum == 3) {
		characterBaseBlock = bg3CharacterBaseBlock;
		screenBaseBlock = bg3ScreenBaseBlock;
		wrapping = bg3Wrapping;
		screenSize = 128 << bg3ScreenSize;
		affX = internalBG3X;
		affY = internalBG3Y;
		pa = (float)((i16)BG3PA) / 255;
		pc = (float)((i16)BG3PC) / 255;
	}

	bool win0HorzFits = win0Right < win0Left;
	bool win1HorzFits = win1Right < win1Left;

	for (int i = 0; i < 240; i++) {
		if (win0Left == i)
			win0HorzFits = true;
		if (win0Right == i)
			win0HorzFits = false;
		if (win1Left == i)
			win1HorzFits = true;
		if (win1Right == i)
			win1HorzFits = false;

		int tilemapIndex = (screenBaseBlock * 0x800) + ((((int)affY & (screenSize - 1)) / 8) * (screenSize / 8)) + (((int)affX & (screenSize - 1)) / 8);

		int tileAddress = (characterBaseBlock * 0x4000) + (vram[tilemapIndex] * 64) + (((int)affY & 7) * 8) + ((int)affX & 7);
		if (tileAddress >= 0x10000)
			break;

		u8 tileData = vram[tileAddress];
		if (!wrapping && (((unsigned int)affY >= (unsigned int)screenSize) || ((unsigned int)affX >= (unsigned int)screenSize))) {
			tileData = 0;
		}

		Pixel *pix = &lineBuffer[bgNum][i];
		pix->inWin0 = false;
		pix->inWin1 = false;
		pix->inWinOut = false;

		if (window0DisplayFlag)
			if (win0HorzFits && win0VertFits)
				pix->inWin0 = true;
		if (window1DisplayFlag)
			if (win1HorzFits && win1VertFits)
				pix->inWin1 = true;
		pix->inWinOut = !(pix->inWin0 || pix->inWin1);

		pix->priority = (tileData == 0) ? -1 : bgNum;
		pix->color = convertColor(paletteColors[tileData]);

		affX += pa;
		affY += pc;
	}
}

void GBAPPU::drawScanline() {
	if (forcedBlank) // I honestly just wanted an excuse to make a mildly cursed for loop
		for (int i = 0; i < 240; framebuffer[currentScanline][i++] = 0xFFFF);

	for (int i = 0; i < 240; i++)
		mergedBuffer[i].color = convertColor(paletteColors[0]);

	switch (bgMode) {
	case 0:
		if (screenDisplayBg0) drawBg<0>();
		if (screenDisplayBg1) drawBg<1>();
		if (screenDisplayBg2) drawBg<2>();
		if (screenDisplayBg3) drawBg<3>();
		if (screenDisplayObj) drawObjects();

		for (int i = 0; i < 240; i++) {
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
		if (screenDisplayBg2) drawBgAff<2>();
		if (screenDisplayObj) drawObjects();

		for (int i = 0; i < 240; i++) {
			// Window
			if (window0DisplayFlag || window1DisplayFlag) {
				for (int layer = 3; layer >= 0; layer--) {
					if (lineBuffer[2][i].inWinOut && winOutBg2Enable && (bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
					if (lineBuffer[1][i].inWinOut && winOutBg1Enable && (bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
					if (lineBuffer[0][i].inWinOut && winOutBg0Enable && (bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
					if (lineBuffer[4 + layer][i].inWinOut && winOutObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
				}

				if (window1DisplayFlag) {
					for (int layer = 3; layer >= 0; layer--) {
						if (lineBuffer[2][i].inWin1 && !lineBuffer[2][i].inWin0 && win1Bg2Enable && (bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
						if (lineBuffer[1][i].inWin1 && !lineBuffer[1][i].inWin0 && win1Bg1Enable && (bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
						if (lineBuffer[0][i].inWin1 && !lineBuffer[0][i].inWin0 && win1Bg0Enable && (bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
						if (lineBuffer[4 + layer][i].inWin1 && !lineBuffer[4 + layer][i].inWin0 && win1ObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
					}
				}

				if (window0DisplayFlag) {
					for (int layer = 3; layer >= 0; layer--) {
						if (lineBuffer[2][i].inWin0 && win0Bg2Enable && (bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
						if (lineBuffer[1][i].inWin0 && win0Bg1Enable && (bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
						if (lineBuffer[0][i].inWin0 && win0Bg0Enable && (bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
						if (lineBuffer[4 + layer][i].inWin0 && win0ObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
					}
				}
			} else {
				for (int layer = 3; layer >= 0; layer--) {
					if ((bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
					if ((bg1Priority == layer) && screenDisplayBg1 && (lineBuffer[1][i].priority != -1)) mergedBuffer[i] = lineBuffer[1][i];
					if ((bg0Priority == layer) && screenDisplayBg0 && (lineBuffer[0][i].priority != -1)) mergedBuffer[i] = lineBuffer[0][i];
					if (screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
				}
			}

			framebuffer[currentScanline][i] = mergedBuffer[i].color;
		}
		break;
	case 2:
		if (screenDisplayBg2) drawBgAff<2>();
		if (screenDisplayBg3) drawBgAff<3>();
		if (screenDisplayObj) drawObjects();

		for (int i = 0; i < 240; i++) {
			// Window
			if (window0DisplayFlag || window1DisplayFlag) {
				for (int layer = 3; layer >= 0; layer--) {
					if (lineBuffer[3][i].inWinOut && winOutBg3Enable && (bg3Priority == layer) && screenDisplayBg3 && (lineBuffer[3][i].priority != -1)) mergedBuffer[i] = lineBuffer[3][i];
					if (lineBuffer[2][i].inWinOut && winOutBg2Enable && (bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
					if (lineBuffer[4 + layer][i].inWinOut && winOutObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
				}

				if (window1DisplayFlag) {
					for (int layer = 3; layer >= 0; layer--) {
						if (lineBuffer[3][i].inWin1 && !lineBuffer[3][i].inWin0 && win1Bg3Enable && (bg3Priority == layer) && screenDisplayBg3 && (lineBuffer[3][i].priority != -1)) mergedBuffer[i] = lineBuffer[3][i];
						if (lineBuffer[2][i].inWin1 && !lineBuffer[2][i].inWin0 && win1Bg2Enable && (bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
						if (lineBuffer[4 + layer][i].inWin1 && !lineBuffer[4 + layer][i].inWin0 && win1ObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
					}
				}

				if (window0DisplayFlag) {
					for (int layer = 3; layer >= 0; layer--) {
						if (lineBuffer[3][i].inWin0 && win0Bg3Enable && (bg3Priority == layer) && screenDisplayBg3 && (lineBuffer[3][i].priority != -1)) mergedBuffer[i] = lineBuffer[3][i];
						if (lineBuffer[2][i].inWin0 && win0Bg2Enable && (bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
						if (lineBuffer[4 + layer][i].inWin0 && win0ObjEnable && screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
					}
				}
			} else {
				for (int layer = 3; layer >= 0; layer--) {
					if ((bg3Priority == layer) && screenDisplayBg3 && (lineBuffer[3][i].priority != -1)) mergedBuffer[i] = lineBuffer[3][i];
					if ((bg2Priority == layer) && screenDisplayBg2 && (lineBuffer[2][i].priority != -1)) mergedBuffer[i] = lineBuffer[2][i];
					if (screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1)) mergedBuffer[i] = lineBuffer[4 + layer][i];
				}
			}

			framebuffer[currentScanline][i] = mergedBuffer[i].color;
		}
		break;
	case 3:
		drawObjects();
		for (int i = 0; i < 240; i++) {
			if (screenDisplayBg2) {
				auto vramIndex = ((currentScanline * 240) + i) * 2;
				u16 vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
				framebuffer[currentScanline][i] = convertColor(vramData);
			}

			for (int layer = 3; layer >= 0; layer--) {
				if (screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1))
					framebuffer[currentScanline][i] = lineBuffer[4 + layer][i].color;
			}
		}
		break;
	case 4:
		drawObjects();
		for (int i = 0; i < 240; i++) {
			if (screenDisplayBg2) {
				auto vramIndex = ((currentScanline * 240) + i) + (displayFrameSelect * 0xA000);
				u8 vramData = vram[vramIndex];
				framebuffer[currentScanline][i] = convertColor(paletteColors[vramData]);
			}

			for (int layer = 3; layer >= 0; layer--) {
				if (screenDisplayObj && (lineBuffer[4 + layer][i].priority != -1))
					framebuffer[currentScanline][i] = lineBuffer[4 + layer][i].color;
			}
		}
		break;
	case 5:
		drawObjects();
		for (int x = 0; x < 240; x++) {
			if ((x < 160) && (currentScanline < 128) && screenDisplayBg2) {
				auto vramIndex = (((currentScanline * 160) + x) * 2) + (displayFrameSelect * 0xA000);
				u16 vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
				framebuffer[currentScanline][x] = convertColor(vramData);
			}

			for (int layer = 3; layer >= 0; layer--) {
				if (screenDisplayObj && (lineBuffer[4 + layer][x].priority != -1))
					framebuffer[currentScanline][x] = lineBuffer[4 + layer][x].color;
			}
		}
		break;
	}

	internalBG2X = (float)((i16)BG2PB) / 255;
	internalBG2Y = (float)((i16)BG2PD) / 255;
	internalBG3X = (float)((i16)BG3PB) / 255;
	internalBG3Y = (float)((i16)BG3PD) / 255;
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
		BG0HOFS = (BG0HOFS & 0x0100) | value;
		break;
	case 0x4000011:
		BG0HOFS = (BG0HOFS & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x4000012:
		BG0VOFS = (BG0VOFS & 0x0100) | value;
		break;
	case 0x4000013:
		BG0VOFS = (BG0VOFS & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x4000014:
		BG1HOFS = (BG1HOFS & 0x0100) | value;
		break;
	case 0x4000015:
		BG1HOFS = (BG1HOFS & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x4000016:
		BG1VOFS = (BG1VOFS & 0x0100) | value;
		break;
	case 0x4000017:
		BG1VOFS = (BG1VOFS & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x4000018:
		BG2HOFS = (BG2HOFS & 0x0100) | value;
		break;
	case 0x4000019:
		BG2HOFS = (BG2HOFS & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x400001A:
		BG2VOFS = (BG2VOFS & 0x0100) | value;
		break;
	case 0x400001B:
		BG2VOFS = (BG2VOFS & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x400001C:
		BG3HOFS = (BG3HOFS & 0x0100) | value;
		break;
	case 0x400001D:
		BG3HOFS = (BG3HOFS & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x400001E:
		BG3VOFS = (BG3VOFS & 0x0100) | value;
		break;
	case 0x400001F:
		BG3VOFS = (BG3VOFS & 0x00FF) | ((value & 1) << 8);
		break;
	case 0x4000020:
		BG2PA = (BG2PA & 0xFF00) | value;
		break;
	case 0x4000021:
		BG2PA = (BG2PA & 0x00FF) | (value << 8);
		break;
	case 0x4000022:
		BG2PB = (BG2PB & 0xFF00) | value;
		break;
	case 0x4000023:
		BG2PB = (BG2PB & 0x00FF) | (value << 8);
		break;
	case 0x4000024:
		BG2PC = (BG2PC & 0xFF00) | value;
		break;
	case 0x4000025:
		BG2PC = (BG2PC & 0x00FF) | (value << 8);
		break;
	case 0x4000026:
		BG2PD = (BG2PD & 0xFF00) | value;
		break;
	case 0x4000027:
		BG2PD = (BG2PD & 0x00FF) | (value << 8);
		break;
	case 0x4000028:
		BG2X = (BG2X & 0xFFFFFF00) | value;
		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 255;
		break;
	case 0x4000029:
		BG2X = (BG2X & 0xFFFF00FF) | (value << 8);
		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 255;
		break;
	case 0x400002A:
		BG2X = (BG2X & 0xFF00FFFF) | (value << 16);
		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 255;
		break;
	case 0x400002B:
		BG2X = (BG2X & 0x00FFFFFF) | ((value & 0x0F) << 24);
		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 255;
		break;
	case 0x400002C:
		BG2Y = (BG2Y & 0xFFFFFF00) | value;
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 255;
		break;
	case 0x400002D:
		BG2Y = (BG2Y & 0xFFFF00FF) | (value << 8);
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 255;
		break;
	case 0x400002E:
		BG2Y = (BG2Y & 0xFF00FFFF) | (value << 16);
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 255;
		break;
	case 0x400002F:
		BG2Y = (BG2Y & 0x00FFFFFF) | ((value & 0x0F) << 24);
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 255;
		break;
	case 0x4000030:
		BG3PA = (BG3PA & 0xFF00) | value;
		break;
	case 0x4000031:
		BG3PA = (BG3PA & 0x00FF) | (value << 8);
		break;
	case 0x4000032:
		BG3PB = (BG3PB & 0xFF00) | value;
		break;
	case 0x4000033:
		BG3PB = (BG3PB & 0x00FF) | (value << 8);
		break;
	case 0x4000034:
		BG3PC = (BG3PC & 0xFF00) | value;
		break;
	case 0x4000035:
		BG3PC = (BG3PC & 0x00FF) | (value << 8);
		break;
	case 0x4000036:
		BG3PD = (BG3PD & 0xFF00) | value;
		break;
	case 0x4000037:
		BG3PD = (BG3PD & 0x00FF) | (value << 8);
		break;
	case 0x4000038:
		BG3X = (BG3X & 0xFFFFFF00) | value;
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 255;
		break;
	case 0x4000039:
		BG3X = (BG3X & 0xFFFF00FF) | (value << 8);
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 255;
		break;
	case 0x400003A:
		BG3X = (BG3X & 0xFF00FFFF) | (value << 16);
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 255;
		break;
	case 0x400003B:
		BG3X = (BG3X & 0x00FFFFFF) | ((value & 0x0F) << 24);
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 255;
		break;
	case 0x400003C:
		BG3Y = (BG3Y & 0xFFFFFF00) | value;
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 255;
		break;
	case 0x400003D:
		BG3Y = (BG3Y & 0xFFFF00FF) | (value << 8);
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 255;
		break;
	case 0x400003E:
		BG3Y = (BG3Y & 0xFF00FFFF) | (value << 16);
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 255;
		break;
	case 0x400003F:
		BG3Y = (BG3Y & 0x00FFFFFF) | ((value & 0x0F) << 24);
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 255;
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