
#ifndef GBA_PPU_H
#define GBA_PPU_H

class GameBoyAdvance;
class GBAPPU {
public:
	GameBoyAdvance& bus;

	GBAPPU(GameBoyAdvance& bus_);
	void reset();
	void cycle();

	template <typename T> T readIO(u32 address);
	template <typename T> void writeIO(u32 address, T value);

	enum ppuStates {
		PPUSTATE_DRAWING,
		PPUSTATE_HBLANK,
		PPUSTATE_VBLANK
	} ppuState;
	void changeMode(enum ppuStates newState);
	void drawScanline();

	std::atomic<bool> updateScreen;
	uint32_t framebuffer[160][240];
	int modeCounter; // TODO: replace with scheduler

	u8 vram[0x18000];

	// MMIO
	union {
		struct {
			u16 bgMode : 3;
			u16 : 1;
			u16 displayFrameSelect : 1;
			u16 hBlankIntervalFree : 1;
			u16 objCahracterVramMapping : 1;
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
		u16 DISPCNT; // 0x04000000
	};
	union {
		struct {
			u16 currentScanline : 8;
			u16 : 8;
		};
		u16 VCOUNT; // 0x04000006
	};
};

#endif