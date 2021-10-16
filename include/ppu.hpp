
#ifndef GBA_PPU_HPP
#define GBA_PPU_HPP

#include <atomic>
#include <cstdio>
#include <cstring>

#include "types.hpp"

class GameBoyAdvance;
class GBAPPU {
public:
	GameBoyAdvance& bus;

	GBAPPU(GameBoyAdvance& bus_);
	void reset();

	template <typename T> T readIO(u32 address);
	template <typename T> void writeIO(u32 address, T value);

	static void lineStartEvent(void *object);
	static void hBlankEvent(void *object);

	void drawScanline();

	std::atomic<bool> updateScreen;
	uint32_t framebuffer[160][240];
	int modeCounter; // TODO: replace with scheduler

	union {
		u8 paletteRam[0x400];
		u16 paletteColors[0x200];
	};
	u8 vram[0x18000];

	// MMIO
	union {
		struct {
			u16 bgMode : 3;
			u16 : 1;
			u16 displayFrameSelect : 1;
			u16 hBlankIntervalFree : 1;
			u16 objCharacterVramMapping : 1;
			u16 forcedBlank : 1;
			u16 screenDisplayBg0 : 1;
			u16 screenDisplayBg1 : 1;
			u16 screenDisplayBg2 : 1;
			u16 screenDisplayBg3 : 1;
			u16 screenDisplayObj : 1;
			u16 window0DisplayFlag : 1;
			u16 window1DisplayFlag : 1;
			u16 objWindowDisplayFlag : 1;
		};
		u16 DISPCNT; // 0x4000000
	};
	union {
		struct {
			u16 vBlankFlag : 1;
			u16 hBlankFlag : 1;
			u16 vCounterFlag : 1;
			u16 vBlankIrqEnable : 1;
			u16 hBlankIrqEnable : 1;
			u16 vCounterIrqEnable : 1;
			u16 : 2;
			u16 vCountSetting : 8;
		};
		u16 DISPSTAT; // 0x4000004
	};
	union {
		struct {
			u16 currentScanline : 8;
			u16 : 8;
		};
		u16 VCOUNT; // 0x4000006
	};
};

#endif