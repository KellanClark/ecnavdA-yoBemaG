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
	
	void stop(u64 cycles);
	static void stopEvent(void *object);

	enum threadEventType {
		STOP,
		START,
		RESET
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
	std::atomic<bool> traceInstructions;
	std::string previousLogLine;
};

#endif