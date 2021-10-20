
#include "ppu.hpp"
#include "scheduler.hpp"
#include "types.hpp"

#define convertColor(x) ((x << 1) | 1)

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