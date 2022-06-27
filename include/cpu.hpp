#ifndef GBA_CPU_HPP
#define GBA_CPU_HPP

#include <atomic>
#include <cstdio>
#include <mutex>
#include <queue>

#include "types.hpp"
#include "arm7tdmi.hpp"
#include "hlebios.hpp"

class GBACPU : public ARM7TDMI {
public:
	bool hleBios;
	GBABIOS bios;

	GBACPU(GameBoyAdvance& bus_);
	void reset();
	void run();

	// Scheduler
	struct Event {
		u64 timeStamp;
		void (*callback)(void*);
		void *userData;
		bool important;
	};
	struct eventSorter {
		bool operator()(const Event &lhs, const Event &rhs);
	};

	void addEvent(u64 cycles, void (*function)(void*), void *pointer, bool important = false);
	void tickScheduler(int cycles);

	u64 currentTime;
	std::priority_queue<Event, std::vector<Event>, eventSorter> eventQueue;

	// Interrupts
	bool uncapFps;
	u16 IE;
	u16 IF;
	bool IME;
	bool halted;
	bool stopped;
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

	// Thread queue
	enum threadEventType {
		STOP,
		START,
		RESET,
		LOAD_BIOS,
		LOAD_ROM,
		UPDATE_KEYINPUT,
		CLEAR_LOG
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
	void addThreadEvent(threadEventType type, void *ptrArg);
	void addThreadEvent(threadEventType type, u64 intArg, void *ptrArg);

	std::atomic<bool> running;
	bool traceInstructions;
	bool logInterrupts;
	std::string previousLogLine;

	static void stopEvent(void *object);
};

#endif
