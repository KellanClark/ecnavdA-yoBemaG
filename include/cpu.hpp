#ifndef GBA_CPU_HPP
#define GBA_CPU_HPP

#include <atomic>
#include <cstdio>
#include <mutex>
#include <queue>

#include "types.hpp"
#include "arm7tdmi.hpp"

class Scheduler;
class GBACPU : public ARM7TDMI {
public:
	GBACPU(GameBoyAdvance& bus_);
	void reset();
	void run();

	void softwareInterrupt(u32 opcode) override;
	void thumbSoftwareInterrupt(u16 opcode) override;

	u16 IE;
	u16 IF;
	bool IME;
	bool halted;
	enum irqType {
		IRQ_VBLANK = 1 << 0,
		IRQ_HBLANK = 1 << 1,
		IRQ_VCOUNT = 1 << 2,
		IRQ_TIMER0 = 1 << 3,
		IRQ_TIMER1 = 1 << 4,
		IRQ_TIMER2 = 1 << 5,
		IRQ_TIMER3 = 1 << 6,
		IRQ_COM = 1 << 7,
		IRQ_DMA0 = 1 << 8,
		IRQ_DMA1 = 1 << 9,
		IRQ_DMA2 = 1 << 10,
		IRQ_DMA3 = 1 << 11,
		IRQ_KEYPAD = 1 << 12,
		IRQ_GAMEPAK = 1 << 13
	};
	void testInterrupt();
	void requestInterrupt(irqType bit);

	static void stopEvent(void *object);

	enum threadEventType {
		STOP,
		START,
		RESET,
		UPDATE_KEYINPUT
	};
	struct threadEvent {
		threadEventType type;
		u64 intArg;
		void *ptrArg;
	};
	std::queue<threadEvent> threadQueue;
	std::mutex threadQueueMutex;
	void processThreadEvents();
	void addThreadEvent(threadEventType type);
	void addThreadEvent(threadEventType type, u64 intArg);
	void addThreadEvent(threadEventType type, u64 intArg, void *ptrArg);

	std::atomic<bool> running;
	bool traceInstructions;
	bool logInterrupts;
	std::string previousLogLine;
};

#endif