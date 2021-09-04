
#include "gba.hpp"

GBAPPU::GBAPPU(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void GBAPPU::reset() {
	memset(framebuffer, 0xFF, sizeof(framebuffer));
	memset(vram, 0, sizeof(vram));

	ppuState = PPUSTATE_DRAWING;
	currentScanline = 0;
}

void GBAPPU::cycle() {
	++modeCounter;
	switch (ppuState) {
	case PPUSTATE_DRAWING:
		if (modeCounter == 960) {
			drawScanline();

			changeMode(PPUSTATE_HBLANK);
		}
		break;
	case PPUSTATE_HBLANK:
		if (modeCounter == 272) {
			if (++currentScanline == 160) {
				changeMode(PPUSTATE_VBLANK);
			} else {
				changeMode(PPUSTATE_DRAWING);
			}
		}
		break;
	case PPUSTATE_VBLANK:
		if ((modeCounter % 1232) == 0)
			++currentScanline;
		if (modeCounter == 83776) {
			currentScanline = 0;
			changeMode(PPUSTATE_DRAWING);
		}
		break;
	}
}

void GBAPPU::changeMode(enum ppuStates newState) {
	switch (newState) {
	case PPUSTATE_DRAWING:
		break;
	case PPUSTATE_HBLANK:
		break;
	case PPUSTATE_VBLANK:
		updateScreen = true;
		break;
	}

	ppuState = newState;
	modeCounter = 0;
}

void GBAPPU::drawScanline() {
	for (int i = 0; i < 240; i++) {
		auto vramIndex = ((currentScanline * 240) + i) * 2;
		//printf("0x%08X\n", vramIndex);
		u16 vramData = (vram[vramIndex + 1] << 8) | vram[vramIndex];
		// Convert XBGR1555 to RGBA8888
		u32 color = (((vramData & 0x7C00) >> 10) << (3 + 8)) | (((vramData & 0x03E0) >> 5) << (3 + 16)) | (((vramData & 0x001F) >> 0) << (3 + 24)) | 0xFF;
		framebuffer[currentScanline][i] = color;
	}
}

template <typename T>
T GBAPPU::readIO(u32 address) {
	if (sizeof(T) == 4) {
		u32 val = readIO<u8>(address++);
		val |= readIO<u8>(address++) << 8;
		val |= readIO<u8>(address++) << 16;
		val |= readIO<u8>(address) << 24;
		return val;
	} else if (sizeof(T) == 2) {
		u16 val = readIO<u8>(address++);
		val |= readIO<u8>(address) << 8;
		return val;
	}

	switch (address) {
	case 0x4000000:
		return (u8)DISPCNT;
	case 0x4000001:
		return (u8)(DISPCNT >> 8);
	case 0x4000006:
		return (u8)VCOUNT;
	case 0x4000007:
		return (u8)(VCOUNT >> 8);
	default:
		return 0;
	}
}
template u8 GBAPPU::readIO<u8>(u32);
template u16 GBAPPU::readIO<u16>(u32);
template u32 GBAPPU::readIO<u32>(u32);

template <typename T>
void GBAPPU::writeIO(u32 address, T value) {
	if (sizeof(T) == 4) {
		writeIO<u8>(address++, (u8)value);
		writeIO<u8>(address++, (u8)(value >> 8));
		writeIO<u8>(address++, (u8)(value >> 16));
		writeIO<u8>(address, (u8)(value >> 24));
		return;
	} else if (sizeof(T) == 2) {
		writeIO<u8>(address++, (u8)value);
		writeIO<u8>(address, (u8)(value >> 8));
		return;
	}

	switch (address) {
	case 0x4000000:
		DISPCNT = (DISPCNT & 0xFF00) | value;
		break;
	case 0x4000001:
		DISPCNT = (DISPCNT & 0x00FF) | (value << 8);
		break;
	}
}
template void GBAPPU::writeIO<u8>(u32, u8);
template void GBAPPU::writeIO<u16>(u32, u16);
template void GBAPPU::writeIO<u32>(u32, u32);