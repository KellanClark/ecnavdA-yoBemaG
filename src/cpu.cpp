
#include "cpu.hpp"
#include "gba.hpp"
#include "scheduler.hpp"

GBACPU::GBACPU(GameBoyAdvance& bus_) : ARM7TDMI(bus_) {
	traceInstructions = true;

	reset();
}

void GBACPU::reset() {
	resetARM7TDMI();
}

void GBACPU::run() { // Emulator thread is run from here
	while (1) {
		processThreadEvents();

		if (running) {
			systemEvents.recalculate = false;
			u64 cyclesToRun = systemEvents.cyclesUntilNextEvent();

			for (u64 i = 0; ((i < cyclesToRun) && !systemEvents.recalculate); i++) {
				if (traceInstructions && (pipelineStage == 3)) {
					std::string disasm = disassemble(reg.R[15] - 8, pipelineOpcode3);
					std::string logLine = fmt::format("0x{:0>8X} | 0x{:0>8X} | {}\n", reg.R[15] - 8, pipelineOpcode3, disasm);
					if (logLine.compare(previousLogLine)) {
						bus.log << logLine;
						previousLogLine = logLine;
					}
				}

				cycle();
				++systemEvents.currentTime;
			}

			if (!systemEvents.recalculate && (systemEvents.eventQueue.top().callback != nullptr)) {
				(*systemEvents.eventQueue.top().callback)(systemEvents.eventQueue.top().userData);
				systemEvents.eventQueue.pop();
			}
			processThreadEvents();
		}
	}
}

void GBACPU::stop(u64 cycles) {
	systemEvents.addEvent(cycles, stopEvent, this);
}


void GBACPU::stopEvent(void *object) {
	static_cast<GBACPU *>(object)->running = false;
}

void GBACPU::processThreadEvents() {
	threadQueueMutex.lock();
	while (!threadQueue.empty()) {
		threadEvent currentEvent = threadQueue.front();
		threadQueue.pop();

		switch (currentEvent.type) {
		case START:
			running = true;
			break;
		case STOP:
			systemEvents.addEvent(currentEvent.intArg, stopEvent, this);
			break;
		case RESET:
			bus.reset();
			break;
		default:
			printf("Unknown thread event:  %d\n", currentEvent.type);
			break;
		}
	}
	threadQueueMutex.unlock();
}

void GBACPU::addThreadEvent(threadEventType type) {
	addThreadEvent(type, 0, nullptr);
}

void GBACPU::addThreadEvent(threadEventType type, u64 intArg) {
	addThreadEvent(type, intArg, nullptr);
}

void GBACPU::addThreadEvent(threadEventType type, u64 intArg, void *ptrArg) {
	threadQueueMutex.lock();
	threadQueue.push(GBACPU::threadEvent{type, intArg, ptrArg});
	threadQueueMutex.unlock();
}