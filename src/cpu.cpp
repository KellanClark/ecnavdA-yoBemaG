
#include "cpu.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include "arm7tdmidisasm.hpp"
#include "types.hpp"
#include <cstdio>

GBACPU::GBACPU(GameBoyAdvance& bus_) : ARM7TDMI(bus_) {
	traceInstructions = false;
	logInterrupts = false;

	reset();
}

void GBACPU::reset() {
	IE = 0;
	IF = 0;
	IME = false;

	resetARM7TDMI();
}

void GBACPU::run() { // Emulator thread is run from here
	while (1) {
		processThreadEvents();

		while (running && !bus.apu.apuBlock) {
			if (!halted) {
				//printf("r0:0x%08X r1:0x%08X r2:0x%08X r3:0x%08X r4:0x%08X r5:0x%08X r6:0x%08X r7:0x%08X r8:0x%08X r9:0x%08X r10:0x%08X r11:0x%08X r12:0x%08X r13:0x%08X r14:0x%08X r15:0x%08X cpsr:0x%08X\n", reg.R[0], reg.R[1], reg.R[2], reg.R[3], reg.R[4], reg.R[5], reg.R[6], reg.R[7], reg.R[8], reg.R[9], reg.R[10], reg.R[11], reg.R[12], reg.R[13], reg.R[14], reg.R[15], reg.CPSR);
				cycle();

				if (traceInstructions && (pipelineStage == 3)) {
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
						bus.log << logLine;
						previousLogLine = logLine;
					}
				}
			} else {
				systemEvents.tickScheduler(1);
			}
		}
	}
}

void GBACPU::softwareInterrupt(u32 opcode) {
	ARM7TDMI::softwareInterrupt(opcode);
}

void GBACPU::thumbSoftwareInterrupt(u16 opcode) {
	ARM7TDMI::thumbSoftwareInterrupt(opcode);
}

void GBACPU::testInterrupt() {
	if (halted && (IE & IF))
		halted = false;

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