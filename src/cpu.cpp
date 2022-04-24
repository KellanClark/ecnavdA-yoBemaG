
#include "cpu.hpp"
#include "fmt/core.h"
#include "gba.hpp"
#include "hlebios.hpp"
#include "arm7tdmidisasm.hpp"
#include "types.hpp"
#include <cstdio>

GBACPU::GBACPU(GameBoyAdvance& bus_) : ARM7TDMI(bus_), bios(*this) {
	hleBios = true;
	bios.processJump = false;
	traceInstructions = false;
	logInterrupts = false;
	uncapFps = false;

	currentTime = 0;
	eventQueue = {};
}

void GBACPU::reset() { // Should only be run once rom is loaded and system is ready
	IE = 0;
	IF = 0;
	IME = false;
	halted = false;
	stopped = false;
	bios.processJump = false;

	resetARM7TDMI();
}

void GBACPU::run() { // Emulator thread is run from here
	while (1) {
		while (!running)
			processThreadEvents();

		if (!halted) {
			//printf("r0:0x%08X r1:0x%08X r2:0x%08X r3:0x%08X r4:0x%08X r5:0x%08X r6:0x%08X r7:0x%08X r8:0x%08X r9:0x%08X r10:0x%08X r11:0x%08X r12:0x%08X r13:0x%08X r14:0x%08X r15:0x%08X cpsr:0x%08X\n", reg.R[0], reg.R[1], reg.R[2], reg.R[3], reg.R[4], reg.R[5], reg.R[6], reg.R[7], reg.R[8], reg.R[9], reg.R[10], reg.R[11], reg.R[12], reg.R[13], reg.R[14], reg.R[15], reg.CPSR);

			while (bios.processJump) [[unlikely]]
				bios.jumpToBios();

			if (traceInstructions) {
				std::string disasm;
				std::string logLine;
				if (reg.thumbMode) {
					disasm = disassembler.disassemble(reg.R[15] - 4, pipelineOpcode3, true);
					logLine = fmt::format("0x{:0>7X} |     0x{:0>4X} | {}\n", reg.R[15] - 4, pipelineOpcode3, disasm);
				} else {
					disasm = disassembler.disassemble(reg.R[15] - 8, pipelineOpcode3, false);
					logLine = fmt::format("0x{:0>7X} | 0x{:0>8X} | {}\n", reg.R[15] - 8, pipelineOpcode3, disasm);
				}

				if (logLine.compare(previousLogLine)) {
					bus.log << fmt::format("r0:0x{:0>8X} r1:0x{:0>8X} r2:0x{:0>8X} r3:0x{:0>8X} r4:0x{:0>8X} r5:0x{:0>8X} r6:0x{:0>8X} r7:0x{:0>8X} r8:0x{:0>8X} r9:0x{:0>8X} r10:0x{:0>8X} r11:0x{:0>8X} r12:0x{:0>8X} r13:0x{:0>8X} r14:0x{:0>8X} r15:0x{:0>8X} cpsr:0x{:0>8X}\n", reg.R[0], reg.R[1], reg.R[2], reg.R[3], reg.R[4], reg.R[5], reg.R[6], reg.R[7], reg.R[8], reg.R[9], reg.R[10], reg.R[11], reg.R[12], reg.R[13], reg.R[14], reg.R[15], reg.CPSR);
					bus.log << logLine;
					previousLogLine = logLine;
				}
			}
			cycle();
		} else {
			// Optimization for halts
			currentTime = eventQueue.top().timeStamp;
			tickScheduler(1);
		}
	}
}

// Scheduler
bool GBACPU::eventSorter::operator()(const Event &lhs, const Event &rhs) {
	return lhs.timeStamp > rhs.timeStamp;
}

void GBACPU::addEvent(u64 cycles, void (*function)(void*), void *pointer, bool important) {
	eventQueue.push(Event{currentTime + cycles, function, pointer, important});
}

void GBACPU::tickScheduler(int cycles) {
	for (int i = 0; i < cycles; i++, currentTime++) {
		while (currentTime >= eventQueue.top().timeStamp) {
			auto callback = eventQueue.top().callback;
			auto userData = eventQueue.top().userData;
			bool important = eventQueue.top().important;

			eventQueue.pop();
			(*callback)(userData);

			if (important) { [[unlikely]]
				do {
					processThreadEvents();
				} while (!(running && (!bus.apu.apuBlock || uncapFps) && !stopped));
			}
		}
	}
}

// Interrupts
void GBACPU::testInterrupt() {
	if (halted && (IE & IF))
		halted = false;
	
	if (stopped && (IE & IF))
		stopped = false;

	if (!reg.irqDisable && IME && (IE & IF))
		processIrq = true;
}

void GBACPU::requestInterrupt(irqType bit) {
	IF |= bit;

	if (logInterrupts) {
		bus.log << "Interrupt requested: ";

		switch (bit) {
		case IRQ_VBLANK: bus.log << "VBlank"; break;
		case IRQ_HBLANK: bus.log << "HBlank"; break;
		case IRQ_VCOUNT: bus.log << "VCount"; break;
		case IRQ_TIMER0: bus.log << "Timer 0"; break;
		case IRQ_TIMER1: bus.log << "Timer 1"; break;
		case IRQ_TIMER2: bus.log << "Timer 2"; break;
		case IRQ_TIMER3: bus.log << "Timer 3"; break;
		case IRQ_COM: bus.log << "Serial"; break;
		case IRQ_DMA0: bus.log << "DMA 0"; break;
		case IRQ_DMA1: bus.log << "DMA 1"; break;
		case IRQ_DMA2: bus.log << "DMA 2"; break;
		case IRQ_DMA3: bus.log << "DMA 3"; break;
		case IRQ_KEYPAD: bus.log << "Keypad"; break;
		case IRQ_GAMEPAK: bus.log << "Gamepak"; break;
		}

		bus.log << "  Line: " << bus.ppu.currentScanline << "\n";
	}

	testInterrupt();
}

// Thread queue
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
			addEvent(currentEvent.intArg, stopEvent, this);
			break;
		case RESET:
			bus.reset();
			break;
		case LOAD_BIOS:
			if (bus.loadBios(*(std::filesystem::path *)currentEvent.ptrArg)) {
				printf("Defaulting to HLE BIOS\n");
				bus.log << "Defaulting to HLE BIOS\n";

				hleBios = true;
			} else {
				hleBios = false;
			}
			break;
		case LOAD_ROM:
			if (bus.loadRom(*(std::filesystem::path *)currentEvent.ptrArg)) {
				threadQueue = {};
			}
			break;
		case UPDATE_KEYINPUT:
			bus.KEYINPUT = currentEvent.intArg & 0x3FF;
			break;
		case CLEAR_LOG:
			bus.log.str("");
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

void GBACPU::addThreadEvent(threadEventType type, void *ptrArg) {
	addThreadEvent(type, 0, ptrArg);
}

void GBACPU::addThreadEvent(threadEventType type, u64 intArg, void *ptrArg) {
	threadQueueMutex.lock();
	threadQueue.push(GBACPU::threadEvent{type, intArg, ptrArg});
	threadQueueMutex.unlock();
}

void GBACPU::stopEvent(void *object) {
	static_cast<GBACPU *>(object)->running = false;
}