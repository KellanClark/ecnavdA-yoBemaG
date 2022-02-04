
#ifndef GBA_TIMER
#define GBA_TIMER

#include "types.hpp"

class GameBoyAdvance;
class GBATIMER {
public:
	GameBoyAdvance& bus;

	GBATIMER(GameBoyAdvance& bus_);
	void reset();

	static void checkOverflowEvent(void *object);
	void checkOverflow();

	template <int timer> u64 getDValue();

    u8 readIO(u32 address);
	void writeIO(u32 address, u8 value);

	u16 initialTIM0D;
	u64 tim0Timestamp;
	u16 initialTIM1D;
	u64 tim1Timestamp;
	u16 initialTIM2D;
	u64 tim2Timestamp;
	u16 initialTIM3D;
	u64 tim3Timestamp;

	u16 TIM0D; // 0x4000100
	union {
		struct {
			u16 tim0Frequency : 2;
			u16 : 4;
			u16 tim0Irq : 1;
			u16 tim0Enable : 1;
			u16 : 8;
		};
		u16 TIM0CNT; // 0x4000102
	};
	u16 TIM1D; // 0x4000104
	union {
		struct {
			u16 tim1Frequency : 2;
			u16 tim1Cascade : 1;
			u16 : 3;
			u16 tim1Irq : 1;
			u16 tim1Enable : 1;
			u16 : 8;
		};
		u16 TIM1CNT; // 0x4000106
	};
	u16 TIM2D; // 0x4000108
	union {
		struct {
			u16 tim2Frequency : 2;
			u16 tim2Cascade : 1;
			u16 : 3;
			u16 tim2Irq : 1;
			u16 tim2Enable : 1;
			u16 : 8;
		};
		u16 TIM2CNT; // 0x400010A
	};
	u16 TIM3D; // 0x400010C
	union {
		struct {
			u16 tim3Frequency : 2;
			u16 tim3Cascade : 1;
			u16 : 3;
			u16 tim3Irq : 1;
			u16 tim3Enable : 1;
			u16 : 8;
		};
		u16 TIM3CNT; // 0x400010E
	};
};

#endif