
#include "scheduler.hpp"
#include <climits>
#include <cstdio>

bool Scheduler::eventSorter::operator()(const Event &lhs, const Event &rhs) {
	return lhs.timeStamp > rhs.timeStamp;
}

Scheduler::Scheduler() {
	reset();
}

void Scheduler::reset() {
	currentTime = 0;
	eventQueue = {};
}

void Scheduler::addEvent(u64 cycles, void (*function)(void*), void *pointer) {
	eventQueue.push(Event{currentTime + cycles, function, pointer});
	recalculate = true;
}

void Scheduler::tickScheduler(int cycles) {
	for (int i = 0; i < cycles; i++) {
		while (currentTime >= eventQueue.top().timeStamp) {
			auto callback = eventQueue.top().callback;
			auto userData = eventQueue.top().userData;
			eventQueue.pop();
			(*callback)(userData);
		}
		++currentTime;
	}
}

Scheduler systemEvents;