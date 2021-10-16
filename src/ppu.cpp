
#include "ppu.hpp"
#include "scheduler.hpp"

// Fast with shifts
//#define color555to8888(x) (((x & 0x7C00) << 1) | ((x & 0x03E0) << 14) | ((x & 0x001F) << 27) | 0xFF)
// Slow with divides and multiplies
#define color555to8888(x) \
	(((int)((((x & 0x7C00) >> 10) / (float)31) * 255) << 8) | \
	((int)((((x & 0x03E0) >> 5) / (float)31) * 255) << 16) | \
	((int)(((x & 0x001F) / (float)31) * 255) << 24) | 0xFF)

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

void GBAPPU::drawScanline() {
	switch (bgMode) {
	case 0x3:
		for (int i = 0; i < 240; i++) {
			auto vramIndex = ((currentScanline * 240) + i) * 2;
			u16 vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
			u32 color = color555to8888(vramData);
			framebuffer[currentScanline][i] = color;
		}
		break;
	case 0x4:
		for (int i = 0; i < 240; i++) {
			auto vramIndex = ((currentScanline * 240) + i) + (displayFrameSelect * 0xA000);
			u8 vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
			u32 color = color555to8888(paletteColors[vramData]);
			framebuffer[currentScanline][i] = color;
		}
		break;
	default:
		for (int i = 0; i < 240; i++)
			framebuffer[currentScanline][i] = 0xFF;
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
	}
}