
#ifndef GBA_PPU_HPP
#define GBA_PPU_HPP

#include <atomic>
#include <array>
#include <cstdio>
#include <cstring>

#include "types.hpp"

class GameBoyAdvance;
class GBAPPU {
public:
	GameBoyAdvance& bus;

	GBAPPU(GameBoyAdvance& bus_);
	void reset();

	static void lineStartEvent(void *object);
	static void hBlankEvent(void *object);

	void drawObjects(int priority);
	template <int mode, int size> int calculateTilemapIndex(int x, int y);
	template <int bgNum> void drawBg();
	void drawScanline();

	u8 readIO(u32 address);
	void writeIO(u32 address, u8 value);

	std::atomic<bool> updateScreen;
	uint16_t framebuffer[160][240];

	struct __attribute__ ((packed)) Object {
		union {
			struct {
				u16 objY : 8;
				u16 objMode : 2;
				u16 gfxMode : 2;
				u16 mosaic : 1;
				u16 bpp : 1;
				u16 shape : 2;
			};
			u16 attribute0;
		};
		union {
			struct {
				u16 objX : 9;
				u16 : 3;
				u16 horizontolFlip : 1;
				u16 verticalFlip : 1;
				u16 size : 2;
			};
			u16 attribute1;
		};
		union {
			struct {
				u16 tileIndex : 10;
				u16 priority : 2;
				u16 palette : 4;
			};
			u16 attribute2;
		};
		u16 unused;
	};

	union {
		u8 paletteRam[0x400];
		u16 paletteColors[0x200];
	};
	u8 vram[0x18000];
	union {
		u8 oam[0x400];
		Object objects[128];
	};

	// MMIO
	union {
		struct {
			u16 bgMode : 3;
			u16 : 1;
			u16 displayFrameSelect : 1;
			u16 hBlankIntervalFree : 1;
			u16 objMappingDimension : 1;
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
	union {
		struct {
			u16 bg0Priority : 2;
			u16 bg0CharacterBaseBlock : 2;
			u16 : 2;
			u16 bg0Mosaic : 1;
			u16 bg0Bpp : 1;
			u16 bg0ScreenBaseBlock : 5;
			u16 : 1;
			u16 bg0ScreenSize : 2;
		};
		u16 BG0CNT; // 0x4000008
	};
	union {
		struct {
			u16 bg1Priority : 2;
			u16 bg1CharacterBaseBlock : 2;
			u16 : 2;
			u16 bg1Mosaic : 1;
			u16 bg1Bpp : 1;
			u16 bg1ScreenBaseBlock : 5;
			u16 : 1;
			u16 bg1ScreenSize : 2;
		};
		u16 BG1CNT; // 0x400000A
	};
	union {
		struct {
			u16 bg2Priority : 2;
			u16 bg2CharacterBaseBlock : 2;
			u16 : 2;
			u16 bg2Mosaic : 1;
			u16 bg2Bpp : 1;
			u16 bg2ScreenBaseBlock : 5;
			u16 bg2Wrapping : 1;
			u16 bg2ScreenSize : 2;
		};
		u16 BG2CNT; // 0x400000C
	};
	union {
		struct {
			u16 bg3Priority : 2;
			u16 bg3CharacterBaseBlock : 2;
			u16 : 2;
			u16 bg3Mosaic : 1;
			u16 bg3Bpp : 1;
			u16 bg3ScreenBaseBlock : 5;
			u16 bg3Wrapping : 1;
			u16 bg3ScreenSize : 2;
		};
		u16 BG3CNT; // 0x400000E
	};
	u16 bg0XOffset; // 0x4000010
	u16 bg0YOffset; // 0x4000012
	u16 bg1XOffset; // 0x4000014
	u16 bg1YOffset; // 0x4000016
	u16 bg2XOffset; // 0x4000018
	u16 bg2YOffset; // 0x400001A
	u16 bg3XOffset; // 0x400001C
	u16 bg3YOffset; // 0x400001E
};

#endif