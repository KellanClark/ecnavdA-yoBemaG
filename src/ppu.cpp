
#include "ppu.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include "types.hpp"
#include <cstdio>
#include <locale>
#include <cmath>

#define convertColor(x) ((x) | 0x8000)

GBAPPU::GBAPPU(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void GBAPPU::reset() {
	// Clear screen
	memset(framebuffer, 0, sizeof(framebuffer));
	updateScreen = true;

	// Clear memory
	memset(paletteRam, 0, sizeof(paletteRam));
	memset(vram, 0, sizeof(vram));
	memset(oam, 0, sizeof(oam));

	win0VertFits = win1VertFits = false;
	internalBG2X = internalBG2Y = internalBG3X = internalBG3Y = 0;

	DISPCNT = 0;
	greenSwap = false;
	DISPSTAT = 0;
	VCOUNT = 0;
	BG0CNT = BG1CNT = BG2CNT = BG3CNT = 0;
	BG0HOFS = BG0VOFS = 0;
	BG1HOFS = BG1VOFS = 0;
	BG2HOFS = BG2VOFS = 0;
	BG3HOFS = BG3VOFS = 0;
	BG2PA = BG2PB = BG2PC = BG2PD = BG2X = BG2Y = 0;
	BG3PA = BG3PB = BG3PC = BG3PD = BG3X = BG3Y = 0;
	WIN0H = WIN1H = WIN0V = WIN1V = 0;
	WININ = WINOUT = 0;
	MOSAIC = 0;

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

		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 256;
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 256;
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 256;
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 256;
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

static const unsigned int objSizeArray[3][4][2] = {
	{{8, 8}, {16, 16}, {32, 32}, {64, 64}},
	{{16, 8}, {32, 8}, {32, 16}, {64, 32}},
	{{8, 16}, {8, 32}, {16, 32}, {32, 64}}
};

inline void GBAPPU::calculateWinObj() {
	drawObjects(5);
}

void GBAPPU::drawObjects(int priority) {
	if (!screenDisplayObj)
		return;
	bool drawWin = priority == 5;
	if (drawWin && !windowObjDisplayFlag)
		return;

	int tileRowAddress = 0;
	int tileDataAddress = 0;
	u8 tileData = 0;

	for (int objNo = 0; objNo < 128; objNo++) {
		Object *obj = &objects[objNo];
		if ((((obj->priority == priority) && (obj->gfxMode != 2)) || (drawWin && (obj->gfxMode == 2))) && (obj->objMode != 2)) {
			ObjectMatrix mat = objectMatrices[obj->affineIndex];
			unsigned int xSize = objSizeArray[obj->shape][obj->size][0];
			unsigned int ySize = objSizeArray[obj->shape][obj->size][1];

			unsigned int x = obj->objX;
			u8 y = currentScanline - obj->objY;
			u8 mosY = (obj->mosaic ? (currentScanline - (currentScanline % (objMosV + 1))) : currentScanline) - obj->objY;
			int yMod = obj->verticalFlip ? (7 - (mosY % 8)) : (mosY % 8);

			float affX = 0;
			float affY = 0;
			float pa = (float)mat.pa / 256;
			float pb = (float)mat.pb / 256;
			float pc = (float)mat.pc / 256;
			float pd = (float)mat.pd / 256;
			if (obj->objMode == 1) { // Affine
				affX = (pb * (y - ((float)ySize / 2))) + (pa * ((float)xSize / -2)) + ((float)xSize / 2);
				affY = (pd * (y - ((float)ySize / 2))) + (pc * ((float)xSize / -2)) + ((float)ySize / 2);
			} else if (obj->objMode == 3) { // Affine double size
				affX = (pb * (y - (float)ySize)) + (pa * ((float)xSize * -1)) + ((float)xSize / 2);
				affY = (pd * (y - (float)ySize)) + (pc * ((float)xSize * -1)) + ((float)ySize / 2);

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

			for (unsigned int relX = 0; relX < (xSize << (obj->objMode == 3)); relX++) {
				if (x < 240) {
					if (mergedBuffer[x].priority == -1) {
						if ((obj->objMode == 1) || (obj->objMode == 3)) {
							unsigned int mosX = floor(affX);
							mosY = floor(affY);
							if (obj->mosaic) {
								mosX = mosX - (mosX % (objMosH + 1));
								mosY = mosY - (mosY % (objMosV + 1));
							}
							if ((mosX < xSize) && (mosY < ySize)) {
								tileDataAddress = 0x10000 + ((obj->tileIndex & ~(1 * obj->bpp)) * 32) + (((((int)mosY / 8) * (objMappingDimension ? (xSize / 8) : (32 >> obj->bpp))) + ((int)mosX / 8)) * (32 << obj->bpp)) + (((unsigned int)mosY & 7) * (4 << obj->bpp)) + (((unsigned int)mosX & 7) / (2 >> obj->bpp));

								tileData = vram[tileDataAddress];
								if (mosX & 1) {
									tileData >>= 4;
								} else {
									tileData &= 0xF;
								}
							} else {
								tileData = 0;
							}
						} else {
							unsigned int mosX = ((obj->mosaic ? (x - (x % (objMosH + 1))) : x) - obj->objX) & 0x1FF;
							if ((mosX >= 0) && (mosX < xSize)) {
								tileRowAddress = 0x10000 + ((obj->tileIndex & ~(1 * obj->bpp)) * 32) + (((((obj->verticalFlip ? (ySize - 1 - mosY) : mosY) / 8) * (objMappingDimension ? (xSize / 8) : (32 >> obj->bpp))) + ((obj->horizontolFlip ? (xSize - 1 - mosX) : mosX) / 8)) * (32 << obj->bpp)) + (yMod * (4 << obj->bpp));
								if (((tileRowAddress <= 0x14000) && (bgMode >= 3)) || (tileRowAddress >= 0x18000))
									break;

								int xMod = obj->horizontolFlip ? (7 - (mosX % 8)) : (mosX % 8);
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
							} else {
								tileData = 0;
							}
						}

						if (drawWin) {
							// Pixel is in object window if non-transparent and not in win0 or win1
							if (tileData &&
								!(window0DisplayFlag && (x >= win0Left) && (x < win0Right) && win0VertFits) &&
								!(window1DisplayFlag && (x >= win1Left) && (x < win1Right) && win1VertFits)) {
								winObjBuffer[x] = true;
							}
						} else {
							Pixel *pix = &mergedBuffer[x];
							pix->inWin0 = false;
							pix->inWin1 = false;
							pix->inWinOut = false;

							if (window0DisplayFlag)
								if ((x >= win0Left) && (x < win0Right) && win0VertFits)
									pix->inWin0 = true;
							if (window1DisplayFlag)
								if ((x >= win1Left) && (x < win1Right) && win1VertFits)
									pix->inWin1 = !pix->inWin0;
							pix->inWinOut = !(pix->inWin0 || pix->inWin1 || winObjBuffer[x]);

							if (tileData) {
								if (window0DisplayFlag || window1DisplayFlag || windowObjDisplayFlag) {
									if ((window0DisplayFlag && win0ObjEnable && pix->inWin0) ||
										(window1DisplayFlag && win1ObjEnable && pix->inWin1) ||
										(windowObjDisplayFlag && winObjObjEnable && winObjBuffer[x]) ||
										(winOutObjEnable && pix->inWinOut)) {
										pix->priority = priority + 4;
										pix->color = paletteColors[0x100 | ((obj->palette << 4) * !obj->bpp) | tileData];
									}
								} else {
									pix->priority = priority + 4;
									pix->color = paletteColors[0x100 | ((obj->palette << 4) * !obj->bpp) | tileData];
								}
							}
						}
					}
				}

				if ((obj->objMode == 1) || (obj->objMode == 3)) {
					affX += pa;
					affY += pc;
				}
				x = (x + 1) & 0x1FF;
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

	switch (size) {
	case 0: // 256x256
		return (baseBlock * 0x800) + (((y % 256) / 8) * 64) + (((x % 256) / 8) * 2);
	case 1: // 512x256
		return ((baseBlock + ((x >> 8) & 1)) * 0x800) + (((y % 256) / 8) * 64) + (((x % 256) / 8) * 2);
	case 2: // 256x512
		return (baseBlock * 0x800) + (((y % 512) / 8) * 64) + (((x % 256) / 8) * 2);
	case 3: // 512x512
		return ((baseBlock + ((x >> 8) & 1)) * 0x800) + (16 * (y & 0x100)) + (((y % 256) / 8) * 64) + (((x % 256) / 8) * 2);
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
	bool mosaic;
	u16 winRegMask;
	switch (bgNum) {
	case 0:
		if (!screenDisplayBg0) return;
		xOffset = BG0HOFS;
		yOffset = BG0VOFS;
		screenSize = bg0ScreenSize;
		bpp = bg0Bpp;
		characterBaseBlock = bg0CharacterBaseBlock;
		mosaic = bg0Mosaic;
		winRegMask = 0x01;
		break;
	case 1:
		if (!screenDisplayBg1) return;
		xOffset = BG1HOFS;
		yOffset = BG1VOFS;
		screenSize = bg1ScreenSize;
		bpp = bg1Bpp;
		characterBaseBlock = bg1CharacterBaseBlock;
		mosaic = bg1Mosaic;
		winRegMask = 0x02;
		break;
	case 2:
		if (!screenDisplayBg2) return;
		xOffset = BG2HOFS;
		yOffset = BG2VOFS;
		screenSize = bg2ScreenSize;
		bpp = bg2Bpp;
		characterBaseBlock = bg2CharacterBaseBlock;
		mosaic = bg2Mosaic;
		winRegMask = 0x04;
		break;
	case 3:
		if (!screenDisplayBg3) return;
		xOffset = BG3HOFS;
		yOffset = BG3VOFS;
		screenSize = bg3ScreenSize;
		bpp = bg3Bpp;
		characterBaseBlock = bg3CharacterBaseBlock;
		mosaic = bg3Mosaic;
		winRegMask = 0x08;
		break;
	}

	int paletteBank = 0;
	bool verticalFlip = false;
	bool horizontolFlip = false;
	int tileIndex = 0;
	int tileRowAddress = 0;

	int x = xOffset;
	int mosX;
	int y = currentScanline + yOffset;
	if (mosaic)
		y -= y % (bgMosV + 1);

	bool win0HorzFits = win0Right < win0Left;
	bool win1HorzFits = win1Right < win1Left;

	for (int i = 0; i < 240; i++, x++) {
		if (win0Left == i)
			win0HorzFits = true;
		if (win0Right == i)
			win0HorzFits = false;
		if (win1Left == i)
			win1HorzFits = true;
		if (win1Right == i)
			win1HorzFits = false;

		if (mergedBuffer[i].priority != -1)
			continue;

		mosX = mosaic ? (x - (x % (bgMosH + 1))) : x;

		{
			int tilemapIndex = (this->*tilemapIndexLUT[(bgNum * 4) + screenSize])(mosX, y);

			u16 tilemapEntry = (vram[tilemapIndex + 1] << 8) | vram[tilemapIndex];
			paletteBank = (tilemapEntry >> 8) & 0xF0;
			verticalFlip = tilemapEntry & 0x0800;
			horizontolFlip = tilemapEntry & 0x0400;
			tileIndex = tilemapEntry & 0x3FF;

			int yMod = verticalFlip ? (7 - (y % 8)) : (y % 8);
			tileRowAddress = (characterBaseBlock * 0x4000) + (tileIndex * (32 << bpp)) + (yMod * (4 << bpp));
		}
		if (tileRowAddress >= 0x10000)
			continue;

		u8 tileData;
		int xMod = horizontolFlip ? (7 - (mosX % 8)) : (mosX % 8);
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

		Pixel *pix = &mergedBuffer[i];
		pix->inWin0 = false;
		pix->inWin1 = false;
		pix->inWinOut = false;

		if (window0DisplayFlag)
			if (win0HorzFits && win0VertFits)
				pix->inWin0 = true;
		if (window1DisplayFlag)
			if (win1HorzFits && win1VertFits)
				pix->inWin1 = !pix->inWin0;
		pix->inWinOut = !(pix->inWin0 || pix->inWin1 || winObjBuffer[i]);

		if (tileData) {
			if (!(window0DisplayFlag || window1DisplayFlag || windowObjDisplayFlag) ||
				(window0DisplayFlag && (WININ & winRegMask) && pix->inWin0) ||
				(window1DisplayFlag && (WININ & (winRegMask << 8)) && pix->inWin1) ||
				(windowObjDisplayFlag && (WINOUT & (winRegMask << 8)) && winObjBuffer[i]) ||
				((WINOUT & winRegMask) && pix->inWinOut)) {
				pix->priority = bgNum;
				pix->color = paletteColors[(paletteBank * !bpp) | tileData];
			}
		}
	}
}

template <int bgNum>
void GBAPPU::drawBgAffine() {
	int characterBaseBlock;
	int screenBaseBlock;
	bool wrapping;
	unsigned int screenSize;
	bool mosaic;
	float affX;
	float affY;
	float pa;
	float pc;
	u16 winRegMask;
	if (bgNum == 2) {
		if (!screenDisplayBg2) return;
		characterBaseBlock = bg2CharacterBaseBlock;
		screenBaseBlock = bg2ScreenBaseBlock;
		wrapping = bg2Wrapping;
		screenSize = 128 << bg2ScreenSize;
		mosaic = bg0Mosaic;
		affX = internalBG2X;
		affY = internalBG2Y;
		pa = (float)BG2PA / 256;
		pc = (float)BG2PC / 256;
		winRegMask = 0x04;
	} else if (bgNum == 3) {
		if (!screenDisplayBg3) return;
		characterBaseBlock = bg3CharacterBaseBlock;
		screenBaseBlock = bg3ScreenBaseBlock;
		wrapping = bg3Wrapping;
		screenSize = 128 << bg3ScreenSize;
		mosaic = bg0Mosaic;
		affX = internalBG3X;
		affY = internalBG3Y;
		pa = (float)BG3PA / 256;
		pc = (float)BG3PC / 256;
		winRegMask = 0x08;
	}

	bool win0HorzFits = win0Right < win0Left;
	bool win1HorzFits = win1Right < win1Left;

	for (int i = 0; i < 240; i++, affX += pa, affY += pc) {
		if (win0Left == i)
			win0HorzFits = true;
		if (win0Right == i)
			win0HorzFits = false;
		if (win1Left == i)
			win1HorzFits = true;
		if (win1Right == i)
			win1HorzFits = false;

		if (mergedBuffer[i].priority != -1)
			continue;

		int mosX = mosaic ? ((int)affX - ((int)affX % (bgMosH + 1))) : (int)affX;
		int mosY = mosaic ? ((int)affY - ((int)affY % (bgMosV + 1))) : (int)affY;
		if (!wrapping && (((unsigned int)mosY >= screenSize) || ((unsigned int)mosX >= screenSize)))
			continue;

		int tilemapIndex = (screenBaseBlock * 0x800) + (((mosY & (screenSize - 1)) / 8) * (screenSize / 8)) + ((mosX & (screenSize - 1)) / 8);
		int tileAddress = (characterBaseBlock * 0x4000) + (vram[tilemapIndex] * 64) + ((mosY & 7) * 8) + (mosX & 7);
		if (tileAddress >= 0x10000)
			continue;
		u8 tileData = vram[tileAddress];

		Pixel *pix = &mergedBuffer[i];
		pix->inWin0 = false;
		pix->inWin1 = false;
		pix->inWinOut = false;

		if (window0DisplayFlag)
			if (win0HorzFits && win0VertFits)
				pix->inWin0 = true;
		if (window1DisplayFlag)
			if (win1HorzFits && win1VertFits)
				pix->inWin1 = !pix->inWin0;
		pix->inWinOut = !(pix->inWin0 || pix->inWin1 || winObjBuffer[i]);

		if (tileData) {
			if (!(window0DisplayFlag || window1DisplayFlag || windowObjDisplayFlag) ||
				(window0DisplayFlag && (WININ & winRegMask) && pix->inWin0) ||
				(window1DisplayFlag && (WININ & (winRegMask << 8)) && pix->inWin1) ||
				(windowObjDisplayFlag && (WINOUT & (winRegMask << 8)) && winObjBuffer[i]) ||
				((WINOUT & winRegMask) && pix->inWinOut)) {
				pix->priority = bgNum;
				pix->color = paletteColors[tileData];
			}
		}
	}
}

template <int mode>
void GBAPPU::drawBgBitmap() {
	if (!screenDisplayBg2) return;
	float affX = internalBG2X;
	float affY = internalBG2Y;
	float pa = (float)BG2PA / 256;
	float pc = (float)BG2PC / 256;
	bool win0HorzFits = win0Right < win0Left;
	bool win1HorzFits = win1Right < win1Left;

	for (int x = 0; x < 240; x++, affX += pa, affY += pc) {
		if (win0Left == x)
			win0HorzFits = true;
		if (win0Right == x)
			win0HorzFits = false;
		if (win1Left == x)
			win1HorzFits = true;
		if (win1Right == x)
			win1HorzFits = false;

		if (mergedBuffer[x].priority != -1)
			continue;

		int mosX = bg2Mosaic ? ((int)affX - ((int)affX % (bgMosH + 1))) : (int)affX;
		int mosY = bg2Mosaic ? ((int)affY - ((int)affY % (bgMosV + 1))) : (int)affY;

		u16 vramData;
		if (mode == 3) {
			if (!bg2Wrapping && (((unsigned int)mosY >= 160) || ((unsigned int)mosX >= 240)))
				continue;

			auto vramIndex = ((mosY * 240) + mosX) * 2;
			vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
		} else if (mode == 4) {
			if (!bg2Wrapping && (((unsigned int)mosY >= 160) || ((unsigned int)mosX >= 240)))
				continue;

			auto vramIndex = ((mosY * 240) + mosX) + (displayFrameSelect * 0xA000);
			vramData = paletteColors[vram[vramIndex]];
		} else if (mode == 5) {
			if (!bg2Wrapping && (((unsigned int)mosY >= 128) || ((unsigned int)mosX >= 160)))
				continue;

			auto vramIndex = (((currentScanline * 160) + x) * 2) + (displayFrameSelect * 0xA000);
			vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
		}

		Pixel *pix = &mergedBuffer[x];
		pix->inWin0 = false;
		pix->inWin1 = false;
		pix->inWinOut = false;

		if (window0DisplayFlag)
			if (win0HorzFits && win0VertFits)
				pix->inWin0 = true;
		if (window1DisplayFlag)
			if (win1HorzFits && win1VertFits)
				pix->inWin1 = !pix->inWin0;
		pix->inWinOut = !(pix->inWin0 || pix->inWin1 || winObjBuffer[x]);

		if (!(window0DisplayFlag || window1DisplayFlag || windowObjDisplayFlag) ||
			(window0DisplayFlag && win0Bg2Enable && pix->inWin0) ||
			(window1DisplayFlag && win1Bg2Enable && pix->inWin1) ||
			(windowObjDisplayFlag && winObjBg2Enable && winObjBuffer[x]) ||
			(winOutBg2Enable && pix->inWinOut)) {
			pix->color = vramData;
			pix->priority = bg2Priority;
		}
	}
}

void GBAPPU::drawScanline() {
	if (forcedBlank) { // I honestly just wanted an excuse to make a mildly cursed for loop
		for (int i = 0; i < 240; framebuffer[currentScanline][i++] = 0xFFFF);
		return;
	}

	for (int i = 0; i < 240; i++) {
		mergedBuffer[i].priority = -1;
		winObjBuffer[i] = false;
	}
	calculateWinObj();

	switch (bgMode) {
	case 0:
		for (int layer = 0; layer < 4; layer++) {
			drawObjects(layer);
			if (bg0Priority == layer) drawBg<0>();
			if (bg1Priority == layer) drawBg<1>();
			if (bg2Priority == layer) drawBg<2>();
			if (bg3Priority == layer) drawBg<3>();
		}
		break;
	case 1:
		for (int layer = 0; layer < 4; layer++) {
			drawObjects(layer);
			if (bg0Priority == layer) drawBg<0>();
			if (bg1Priority == layer) drawBg<1>();
			if (bg2Priority == layer) drawBgAffine<2>();
		}
		break;
	case 2:
		for (int layer = 0; layer < 4; layer++) {
			drawObjects(layer);
			if (bg2Priority == layer) drawBgAffine<2>();
			if (bg3Priority == layer) drawBgAffine<3>();
		}
		break;
	case 3:
		for (int layer = 0; layer < 4; layer++) {
			drawObjects(layer);
			if (bg2Priority == layer) drawBgBitmap<3>();
		}
		break;
	case 4:
		for (int layer = 0; layer < 4; layer++) {
			drawObjects(layer);
			if (bg2Priority == layer) drawBgBitmap<4>();
		}
		break;
	case 5:
		for (int layer = 0; layer < 4; layer++) {
			drawObjects(layer);
			if (bg2Priority == layer) drawBgBitmap<5>();
		}
		break;
	}

	for (int i = 0; i < 240; i++) { // Copy scanline buffer to main framebuffer
		framebuffer[currentScanline][i] = convertColor((mergedBuffer[i].priority == -1) ? paletteColors[0] : mergedBuffer[i].color);
	}

	if (greenSwap) { // Convert BGRbgr pattern to BgRbGr
		for (int i = 0; i < 240; i += 2) {
			u16 left = framebuffer[currentScanline][i];
			u16 right = framebuffer[currentScanline][i + 1];

			framebuffer[currentScanline][i] = (left & ~(0x1F << 5)) | (right & (0x1F << 5));
			framebuffer[currentScanline][i + 1] = (right & ~(0x1F << 5)) | (left & (0x1F << 5));
		}
	}

	internalBG2X += (float)BG2PB / 256;
	internalBG2Y += (float)BG2PD / 256;
	internalBG3X += (float)BG3PB / 256;
	internalBG3Y += (float)BG3PD / 256;
}

u8 GBAPPU::readIO(u32 address) {
	switch (address) {
	case 0x4000000:
		return (u8)DISPCNT;
	case 0x4000001:
		return (u8)(DISPCNT >> 8);
	case 0x4000002:
		return (u8)greenSwap;
	case 0x4000003:
		return 0;
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
		return bus.openBus<u8>(address);
	}
}

void GBAPPU::writeIO(u32 address, u8 value) {
	switch (address) {
	case 0x4000000:
		DISPCNT = (DISPCNT & 0xFF00) | (value & 0xF7);
		break;
	case 0x4000001:
		DISPCNT = (DISPCNT & 0x00FF) | (value << 8);
		break;
	case 0x4000002:
		greenSwap = value & 1;
		break;
	case 0x4000004:
		DISPSTAT = (DISPSTAT & 0xFFC7) | (value & 0x38);
		break;
	case 0x4000005:
		DISPSTAT = (DISPSTAT & 0x00FF) | (value << 8);
		break;
	case 0x4000008:
		BG0CNT = (BG0CNT & 0xFF00) | value;
		break;
	case 0x4000009:
		BG0CNT = (BG0CNT & 0x00FF) | ((value & 0xDF) << 8);
		break;
	case 0x400000A:
		BG1CNT = (BG1CNT & 0xFF00) | value;
		break;
	case 0x400000B:
		BG1CNT = (BG1CNT & 0x00FF) | ((value & 0xDF) << 8);
		break;
	case 0x400000C:
		BG2CNT = (BG2CNT & 0xFF00) | value;
		break;
	case 0x400000D:
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
		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 256;
		break;
	case 0x4000029:
		BG2X = (BG2X & 0xFFFF00FF) | (value << 8);
		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 256;
		break;
	case 0x400002A:
		BG2X = (BG2X & 0xFF00FFFF) | (value << 16);
		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 256;
		break;
	case 0x400002B:
		BG2X = (BG2X & 0x00FFFFFF) | ((value & 0x0F) << 24);
		internalBG2X = (float)((i32)(BG2X << 4) >> 4) / 256;
		break;
	case 0x400002C:
		BG2Y = (BG2Y & 0xFFFFFF00) | value;
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 256;
		break;
	case 0x400002D:
		BG2Y = (BG2Y & 0xFFFF00FF) | (value << 8);
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 256;
		break;
	case 0x400002E:
		BG2Y = (BG2Y & 0xFF00FFFF) | (value << 16);
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 256;
		break;
	case 0x400002F:
		BG2Y = (BG2Y & 0x00FFFFFF) | ((value & 0x0F) << 24);
		internalBG2Y = (float)((i32)(BG2Y << 4) >> 4) / 256;
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
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 256;
		break;
	case 0x4000039:
		BG3X = (BG3X & 0xFFFF00FF) | (value << 8);
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 256;
		break;
	case 0x400003A:
		BG3X = (BG3X & 0xFF00FFFF) | (value << 16);
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 256;
		break;
	case 0x400003B:
		BG3X = (BG3X & 0x00FFFFFF) | ((value & 0x0F) << 24);
		internalBG3X = (float)((i32)(BG3X << 4) >> 4) / 256;
		break;
	case 0x400003C:
		BG3Y = (BG3Y & 0xFFFFFF00) | value;
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 256;
		break;
	case 0x400003D:
		BG3Y = (BG3Y & 0xFFFF00FF) | (value << 8);
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 256;
		break;
	case 0x400003E:
		BG3Y = (BG3Y & 0xFF00FFFF) | (value << 16);
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 256;
		break;
	case 0x400003F:
		BG3Y = (BG3Y & 0x00FFFFFF) | ((value & 0x0F) << 24);
		internalBG3Y = (float)((i32)(BG3Y << 4) >> 4) / 256;
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
	case 0x400004C:
		MOSAIC = (MOSAIC & 0xFF00) | value;
		break;
	case 0x400004D:
		MOSAIC = (MOSAIC & 0x00FF) | (value << 8);
		break;
	}
}