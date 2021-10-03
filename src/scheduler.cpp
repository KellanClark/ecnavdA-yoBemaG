
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
	eventQueue.push(Event{ULLONG_MAX, nullptr, nullptr}); // Always keep CPU running. Will segfault if left for ~34,841 years. Pls don't.
}

u64 Scheduler::cyclesUntilNextEvent() {
	return eventQueue.top().timeStamp - currentTime;
}

void Scheduler::addEvent(u64 cycles, void (*function)(void*), void *pointer) {
	eventQueue.push(Event{currentTime + cycles, function, pointer});
	recalculate = true;
}

Scheduler systemEvents;