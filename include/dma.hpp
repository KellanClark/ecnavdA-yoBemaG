
#ifndef GBA_DMA
#define GBA_DMA

#include "types.hpp"

class GameBoyAdvance;
class GBADMA {
public:
	GameBoyAdvance& bus;

	GBADMA(GameBoyAdvance& bus_);
	void reset();

	static void dmaEndEvent(void *object);
	void dmaEnd();

	void onVBlank();
	void onHBlank();
	void checkDma();
	template <int channel> void doDma();

    u8 readIO(u32 address);
	void writeIO(u32 address, u8 value);

	bool logDma;

	int currentDma;
	bool dma0Queued;
	bool dma1Queued;
	bool dma2Queued;
	bool dma3Queued;

	union DmaControlBits {
		struct {
			u32 numTransfers : 16;
			u32 : 5;
			u32 dstControl : 2;
			u32 srcControl : 2;
			u32 repeat : 1;
			u32 transferSize : 1;
			u32 : 1;
			u32 timing : 2;
			u32 requestInterrupt : 1;
			u32 enable : 1;
		};
		u32 raw;
	};

	u32 internalDMA0SAD;
	u32 internalDMA0DAD;
	DmaControlBits internalDMA0CNT;

	u32 internalDMA1SAD;
	u32 internalDMA1DAD;
	DmaControlBits internalDMA1CNT;

	u32 internalDMA2SAD;
	u32 internalDMA2DAD;
	DmaControlBits internalDMA2CNT;

	u32 internalDMA3SAD;
	u32 internalDMA3DAD;
	DmaControlBits internalDMA3CNT;

	u32 DMA0SAD; // 0x40000B0
	u32 DMA0DAD; // 0x40000B4
	DmaControlBits DMA0CNT; // 0x40000B8
	u32 DMA1SAD; // 0x40000BC
	u32 DMA1DAD; // 0x40000C0
	DmaControlBits DMA1CNT; // 0x40000C4
	u32 DMA2SAD; // 0x40000C8
	u32 DMA2DAD; // 0x40000CC
	DmaControlBits DMA2CNT; // 0x40000D0
	u32 DMA3SAD; // 0x40000D4
	u32 DMA3DAD; // 0x40000D8
	DmaControlBits DMA3CNT; // 0x40000DC
};

#endif