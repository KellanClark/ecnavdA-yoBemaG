
#include "cpu.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include "types.hpp"

GBACPU::GBACPU(GameBoyAdvance& bus_) : ARM7TDMI(bus_) {
	traceInstructions = false;

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
					std::string disasm;
					std::string logLine;
					if (reg.thumbMode) {
						disasm = disassemble(reg.R[15] - 4, pipelineOpcode3, true);
						logLine = fmt::format("0x{:0>7X} |     0x{:0>4X} | {}\n", reg.R[15] - 4, pipelineOpcode3, disasm);
					} else {
						disasm = disassemble(reg.R[15] - 8, pipelineOpcode3, false);
						logLine = fmt::format("0x{:0>7X} | 0x{:0>8X} | {}\n", reg.R[15] - 8, pipelineOpcode3, disasm);
					}

					if (logLine.compare(previousLogLine)) {
						bus.log << logLine;
						previousLogLine = logLine;
					}
				}

				cycle();
				++systemEvents.currentTime;
				//printf("r0:0x%08X r1:0x%08X r2:0x%08X r3:0x%08X r4:0x%08X r5:0x%08X r6:0x%08X r7:0x%08X r8:0x%08X r9:0x%08X r10:0x%08X r11:0x%08X r12:0x%08X r13:0x%08X r14:0x%08X r15:0x%08X cpsr:0x%08X\n\n", reg.R[0], reg.R[1], reg.R[2], reg.R[3], reg.R[4], reg.R[5], reg.R[6], reg.R[7], reg.R[8], reg.R[9], reg.R[10], reg.R[11], reg.R[12], reg.R[13], reg.R[14], reg.R[15], reg.CPSR);
			}

			if (!systemEvents.recalculate && (systemEvents.eventQueue.top().callback != nullptr)) {
				(*systemEvents.eventQueue.top().callback)(systemEvents.eventQueue.top().userData);
				systemEvents.eventQueue.pop();
			}
			processThreadEvents();
		}
	}
}

void GBACPU::requestInterrupt(irqType bit) {
	IF |= bit;
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
		case UPDATE_KEYINPUT:
			bus.KEYINPUT = currentEvent.intArg & 0x3FF;
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