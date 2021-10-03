#ifndef GBA_SCHEDULER_HPP
#define GBA_SCHEDULER_HPP

#include <queue>

#include "types.hpp"

class Scheduler {
public:
	struct Event {
		u64 timeStamp;
		void (*callback)(void*);
		void *userData;
	};
	struct eventSorter {
		bool operator()(const Event &lhs, const Event &rhs);
	};

	Scheduler();
	void reset();
	u64 cyclesUntilNextEvent();
	void addEvent(u64 cycles, void (*function)(void*), void *pointer);

	u64 currentTime;
	std::priority_queue<Event, std::vector<Event>, eventSorter> eventQueue;
	bool recalculate;
};

extern Scheduler systemEvents;

#endif