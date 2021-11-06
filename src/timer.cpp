
#include "timer.hpp"
#include "scheduler.hpp"
#include "gba.hpp"

GBATIMER::GBATIMER(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void GBATIMER::reset() {
	TIM0D = TIM0CNT = initialTIM0D = 0;
	TIM1D = TIM1CNT = initialTIM1D = 0;
	TIM2D = TIM2CNT = initialTIM2D = 0;
	TIM3D = TIM3CNT = initialTIM3D = 0;
}

void GBATIMER::checkOverflowEvent(void *object) {
	static_cast<GBATIMER *>(object)->checkOverflow();
}

void GBATIMER::checkOverflow() {
	bool previousOverflow = false;
	if (tim0Enable) {
		if (getDValue<0>() > 0xFFFF) { // Overflow
			if (tim0Irq)
				bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER0);

			TIM0D = initialTIM0D;
			tim0Timestamp = systemEvents.currentTime;
			if (!tim0Cascade)
				systemEvents.addEvent((0x10000 - TIM0D) * (tim0Frequency ? (16 << (2 * tim0Frequency)) : 1), &checkOverflowEvent, this);
			
			previousOverflow = true;
		} else {
			previousOverflow = false;
		}
	}
	if (tim1Enable) {
		if (getDValue<1>() > 0xFFFF) { // Overflow
			if (tim1Irq)
				bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER1);

			TIM1D = initialTIM1D;
			tim1Timestamp = systemEvents.currentTime;
			systemEvents.addEvent((0x10000 - TIM1D) * (tim1Frequency ? (16 << (2 * tim1Frequency)) : 1), &checkOverflowEvent, this);

			previousOverflow = true;
		} else if (tim1Cascade && previousOverflow) { // Cascade
			if (++TIM1D == 0) { // Cascade Overflow
				if (tim1Irq)
					bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER1);

				previousOverflow = true;
			}
		} else {
			previousOverflow = false;
		}
	}
	if (tim2Enable) {
		if (getDValue<2>() > 0xFFFF) { // Overflow
			if (tim2Irq)
				bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER2);

			TIM2D = initialTIM2D;
			tim2Timestamp = systemEvents.currentTime;
			systemEvents.addEvent((0x10000 - TIM2D) * (tim2Frequency ? (16 << (2 * tim2Frequency)) : 1), &checkOverflowEvent, this);

			previousOverflow = true;
		} else if (tim2Cascade && previousOverflow) { // Cascade
			if (++TIM2D == 0) { // Cascade Overflow
				if (tim2Irq)
					bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER2);

				previousOverflow = true;
			}
		} else {
			previousOverflow = false;
		}
	}
	if (tim3Enable) {
		if (getDValue<3>() > 0xFFFF) { // Overflow
			if (tim3Irq)
				bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER3);

			TIM3D = initialTIM3D;
			tim3Timestamp = systemEvents.currentTime;
			systemEvents.addEvent((0x10000 - TIM3D) * (tim3Frequency ? (16 << (2 * tim3Frequency)) : 1), &checkOverflowEvent, this);
		} else if (tim3Cascade && previousOverflow) { // Cascade
			if (++TIM3D == 0) { // Cascade Overflow
				if (tim3Irq)
					bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER3);
			}
		}
	}
}

template <int timer>
u64 GBATIMER::getDValue() {
	switch (timer) {
	case 0:
		return TIM0D + ((tim0Enable && !tim0Cascade) * ((systemEvents.currentTime - tim0Timestamp) / (tim0Frequency ? (16 << (2 * tim0Frequency)) : 1)));
	case 1:
		return TIM1D + ((tim1Enable && !tim1Cascade) * ((systemEvents.currentTime - tim1Timestamp) / (tim1Frequency ? (16 << (2 * tim1Frequency)) : 1)));
	case 2:
		return TIM2D + ((tim2Enable && !tim2Cascade) * ((systemEvents.currentTime - tim2Timestamp) / (tim2Frequency ? (16 << (2 * tim2Frequency)) : 1)));
	case 3:
		return TIM3D + ((tim3Enable && !tim3Cascade) * ((systemEvents.currentTime - tim3Timestamp) / (tim3Frequency ? (16 << (2 * tim3Frequency)) : 1)));
	}
}

u8 GBATIMER::readIO(u32 address) {
	switch (address) {
	case 0x4000100:
		return (u8)getDValue<0>();
	case 0x4000101:
		return (u8)(getDValue<0>() >> 8);
	case 0x4000102:
		return TIM0CNT;
	case 0x4000104:
		return (u8)getDValue<1>();
	case 0x4000105:
		return (u8)(getDValue<1>() >> 8);
	case 0x4000106:
		return TIM1CNT;
	case 0x4000108:
		return (u8)getDValue<2>();
	case 0x4000109:
		return (u8)(getDValue<2>() >> 8);
	case 0x400010A:
		return TIM2CNT;
	case 0x400010C:
		return (u8)getDValue<3>();
	case 0x400010D:
		return (u8)(getDValue<3>() >> 8);
	case 0x400010E:
		return TIM3CNT;
	default:
		return 0;
	}
}

void GBATIMER::writeIO(u32 address, u8 value) {
	switch (address) {
	case 0x4000100:
		initialTIM0D = (initialTIM0D & 0xFF00) | value;
		break;
	case 0x4000101:
		initialTIM0D = (initialTIM0D & 0x00FF) | (value << 8);
		break;
	case 0x4000102:
		if ((value & 0x80) && !(TIM0CNT & 0x80)) { // Enabling the timer
			TIM0D = initialTIM0D;
			tim0Timestamp = systemEvents.currentTime;
		}
		if ((!(value & 0x80) && (TIM0CNT & 0x80)) || ((value & 4) && !(TIM0CNT & 4))) // Disabling the timer or enabling cascade
			TIM0D = getDValue<0>();
		if (((value & 3) != (TIM0CNT & 3)) && !(value & 4)) { // Changing frequency with cascade off
			TIM0D = getDValue<0>();
			tim0Timestamp = systemEvents.currentTime;
		}
		if (!(value & 4) && (TIM0CNT & 4)) // Disabling cascade
			tim0Timestamp = systemEvents.currentTime;

		TIM0CNT = (value & 0xC7);

		if (tim0Timestamp == systemEvents.currentTime && tim0Enable && !tim0Cascade)
			systemEvents.addEvent((0x10000 - TIM0D) * (tim0Frequency ? (16 << (2 * tim0Frequency)) : 1), &checkOverflowEvent, this);
		break;
	case 0x4000104:
		initialTIM1D = (initialTIM1D & 0xFF00) | value;
		break;
	case 0x4000105:
		initialTIM1D = (initialTIM1D & 0x00FF) | (value << 8);
		break;
	case 0x4000106:
		if ((value & 0x80) && !(TIM1CNT & 0x80)) { // Enabling the timer
			TIM1D = initialTIM1D;
			tim1Timestamp = systemEvents.currentTime;
		}
		if ((!(value & 0x80) && (TIM1CNT & 0x80)) || ((value & 4) && !(TIM1CNT & 4))) // Disabling the timer or enabling cascade
			TIM1D = getDValue<1>();
		if (((value & 3) != (TIM1CNT & 3)) && !(value & 4)) { // Changing frequency with cascade off
			TIM1D = getDValue<1>();
			tim1Timestamp = systemEvents.currentTime;
		}
		if (!(value & 4) && (TIM1CNT & 4)) // Disabling cascade
			tim1Timestamp = systemEvents.currentTime;

		TIM1CNT = (value & 0xC7);

		if (tim1Timestamp == systemEvents.currentTime && tim1Enable && !tim1Cascade)
			systemEvents.addEvent((0x10000 - TIM1D) * (tim1Frequency ? (16 << (2 * tim1Frequency)) : 1), &checkOverflowEvent, this);
		break;
	case 0x4000108:
		initialTIM2D = (initialTIM2D & 0xFF00) | value;
		break;
	case 0x4000109:
		initialTIM2D = (initialTIM2D & 0x00FF) | (value << 8);
		break;
	case 0x400010A:
		if ((value & 0x80) && !(TIM2CNT & 0x80)) { // Enabling the timer
			TIM2D = initialTIM2D;
			tim2Timestamp = systemEvents.currentTime;
		}
		if ((!(value & 0x80) && (TIM2CNT & 0x80)) || ((value & 4) && !(TIM2CNT & 4))) // Disabling the timer or enabling cascade
			TIM2D = getDValue<2>();
		if (((value & 3) != (TIM2CNT & 3)) && !(value & 4)) { // Changing frequency with cascade off
			TIM2D = getDValue<2>();
			tim2Timestamp = systemEvents.currentTime;
		}
		if (!(value & 4) && (TIM2CNT & 4)) // Disabling cascade
			tim2Timestamp = systemEvents.currentTime;

		TIM2CNT = (value & 0xC7);

		if (tim2Timestamp == systemEvents.currentTime && tim2Enable && !tim2Cascade)
			systemEvents.addEvent((0x10000 - TIM2D) * (tim2Frequency ? (16 << (2 * tim2Frequency)) : 1), &checkOverflowEvent, this);
		break;
	case 0x400010C:
		initialTIM3D = (initialTIM3D & 0xFF00) | value;
		break;
	case 0x400010D:
		initialTIM3D = (initialTIM3D & 0x00FF) | (value << 8);
		break;
	case 0x400010E:
		if ((value & 0x80) && !(TIM3CNT & 0x80)) { // Enabling the timer
			TIM3D = initialTIM3D;
			tim3Timestamp = systemEvents.currentTime;
		}
		if ((!(value & 0x80) && (TIM3CNT & 0x80)) || ((value & 4) && !(TIM3CNT & 4))) // Disabling the timer or enabling cascade
			TIM3D = getDValue<3>();
		if (((value & 3) != (TIM3CNT & 3)) && !(value & 4)) { // Changing frequency with cascade off
			TIM3D = getDValue<3>();
			tim3Timestamp = systemEvents.currentTime;
		}
		if (!(value & 4) && (TIM3CNT & 4)) // Disabling cascade
			tim3Timestamp = systemEvents.currentTime;

		TIM3CNT = (value & 0xC7);

		if (tim3Timestamp == systemEvents.currentTime && tim3Enable && !tim3Cascade)
			systemEvents.addEvent((0x10000 - TIM3D) * (tim3Frequency ? (16 << (2 * tim3Frequency)) : 1), &checkOverflowEvent, this);
		break;
	}
}