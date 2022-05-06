
#include "dma.hpp"
#include "fmt/core.h"
#include "types.hpp"
#include "gba.hpp"
#include <cstdio>

GBADMA::GBADMA(GameBoyAdvance& bus_) : bus(bus_) {
	logDma = false;

	reset();
}

void GBADMA::reset() {
	currentDma = -1;
	dma0Queued = dma1Queued = dma2Queued = dma3Queued = false;

	internalDMA0SAD = internalDMA0DAD = internalDMA0CNT.raw = 0;
	internalDMA1SAD = internalDMA1DAD = internalDMA1CNT.raw = 0;
	internalDMA2SAD = internalDMA2DAD = internalDMA2CNT.raw = 0;
	internalDMA3SAD = internalDMA3DAD = internalDMA3CNT.raw = 0;

	DMA0SAD = DMA0DAD = DMA0CNT.raw = 0;
	DMA1SAD = DMA1DAD = DMA1CNT.raw = 0;
	DMA2SAD = DMA2DAD = DMA2CNT.raw = 0;
	DMA3SAD = DMA3DAD = DMA3CNT.raw = 0;
}

void GBADMA::dmaCheckEvent(void *object) {
	static_cast<GBADMA *>(object)->checkDma();
}

void GBADMA::onVBlank() {
	if ((currentDma != 0) && internalDMA0CNT.enable && (internalDMA0CNT.timing == 1))
		dma0Queued = true;
	if ((currentDma != 1) && internalDMA1CNT.enable && (internalDMA1CNT.timing == 1))
		dma1Queued = true;
	if ((currentDma != 2) && internalDMA2CNT.enable && (internalDMA2CNT.timing == 1))
		dma2Queued = true;
	if ((currentDma != 3) && internalDMA3CNT.enable && (internalDMA3CNT.timing == 1))
		dma3Queued = true;

	checkDma();
}

void GBADMA::onHBlank() {
	if ((currentDma != 0) && internalDMA0CNT.enable && (internalDMA0CNT.timing == 2))
		dma0Queued = true;
	if ((currentDma != 1) && internalDMA1CNT.enable && (internalDMA1CNT.timing == 2))
		dma1Queued = true;
	if ((currentDma != 2) && internalDMA2CNT.enable && (internalDMA2CNT.timing == 2))
		dma2Queued = true;
	if ((currentDma != 3) && internalDMA3CNT.enable && (internalDMA3CNT.timing == 2))
		dma3Queued = true;

	checkDma();
}

void GBADMA::onFifoA() {
	if ((currentDma != 1) && internalDMA1CNT.enable && (internalDMA1CNT.timing == 3))
		dma1Queued = true;

	checkDma();
}

void GBADMA::onFifoB() {
	if ((currentDma != 2) && internalDMA2CNT.enable && (internalDMA2CNT.timing == 3))
		dma2Queued = true;

	checkDma();
}

void GBADMA::checkDma() {
	if (currentDma == -1) {
		if (internalDMA0CNT.enable && dma0Queued) {
			doDma<0>();
		} else if (internalDMA1CNT.enable && dma1Queued) {
			doDma<1>();
		} else if (internalDMA2CNT.enable && dma2Queued) {
			doDma<2>();
		} else if (internalDMA3CNT.enable && dma3Queued) {
			doDma<3>();
		}
	}
}

template <int channel>
void GBADMA::doDma() {
	bus.internalCycle(1);

	u32 *sourceAddress;
	u32 *destinationAddress;
	DmaControlBits *control;
	u32 *openBus;
	switch (channel) {
	case 0:
		sourceAddress = &internalDMA0SAD;
		destinationAddress = &internalDMA0DAD;
		control = &internalDMA0CNT;
		currentDma = 0;
		dma0Queued = false;
		openBus = &dma0OpenBus;
		break;
	case 1:
		sourceAddress = &internalDMA1SAD;
		destinationAddress = &internalDMA1DAD;
		control = &internalDMA1CNT;
		currentDma = 1;
		dma1Queued = false;
		openBus = &dma1OpenBus;
		break;
	case 2:
		sourceAddress = &internalDMA2SAD;
		destinationAddress = &internalDMA2DAD;
		control = &internalDMA2CNT;
		currentDma = 2;
		dma2Queued = false;
		openBus = &dma2OpenBus;
		break;
	case 3:
		sourceAddress = &internalDMA3SAD;
		destinationAddress = &internalDMA3DAD;
		control = &internalDMA3CNT;
		currentDma = 3;
		dma3Queued = false;
		openBus = &dma3OpenBus;
		break;
	}

	//if ((*sourceAddress >= 0x8000000) && (*destinationAddress >= 0x8000000))
	//	bus.internalCycle(2);

	int length = control->numTransfers;
	if (length == 0)
		length = (channel == 3) ? 0x10000 : 0x4000;

	// TODO: Create shadow DMACNT registers
	// TODO: Find if force alignment is kept in internal src/dst registers
	if ((control->srcControl < 3) && (*sourceAddress >= 0x8000000) && (*sourceAddress < 0xE000000))
		control->srcControl = 0;
	if (((channel == 1) || (channel == 2)) && (control->timing == 3)) {
		length = 4;
		control->dstControl = 2;
	}

	if (logDma) {
		bus.log << fmt::format("DMA Channel {} from 0x{:0>7X} to 0x{:0>7X} of length 0x{:0>4X} with control = 0x{:0>4X}\n", channel, *sourceAddress, *destinationAddress, length, control->raw >> 16);
		bus.log << "Request Interrupt: " << (control->requestInterrupt ? "True" : "False") << "  Timing: ";
		switch (control->timing) {
		case 0: bus.log << "Immediately"; break;
		case 1: bus.log << "VBlank"; break;
		case 2: bus.log << "HBlank"; break;
		case 3: bus.log << "Audio"; break;
		}
		bus.log << "  Chunk Size: " << (16 << control->transferSize) << "-bit  Repeat: " << (control->repeat ? "True" : "False") << "\n";
		bus.log << "Source Adjustment: ";
		switch (control->srcControl) {
		case 0: bus.log << "Increment"; break;
		case 1: bus.log << "Decrement"; break;
		case 2: bus.log << "Fixed"; break;
		}
		bus.log << "  Destination Adjustment: ";
		switch (control->dstControl) {
		case 0: bus.log << "Increment"; break;
		case 1: bus.log << "Decrement"; break;
		case 2: bus.log << "Fixed"; break;
		case 3: bus.log << "Increment/Reload"; break;
		}
		bus.log << "\nScanline " << bus.ppu.currentScanline << "\n";
	}

	bool hasTransferred = false;
	if (control->transferSize) { // 32 bit
		for (int i = 0; i < length; i++) {
			if (*sourceAddress < 0x2000000) {
				bus.write<u32>(*destinationAddress & ~3, *openBus, hasTransferred);
			} else {
				u32 data = bus.read<u32, false>(*sourceAddress & ~3, hasTransferred);
				if (!hasTransferred && (*sourceAddress >= 0x8000000) && (*destinationAddress >= 0x8000000))
					hasTransferred = true;
				bus.write<u32>(*destinationAddress & ~3, data, hasTransferred);
				*openBus = data;
			}

			if ((control->dstControl == 0) || (control->dstControl == 3)) { // Increment
				*destinationAddress += 4;
			} else if (control->dstControl == 1) { // Decrement
				*destinationAddress -= 4;
			}

			if (control->srcControl == 0) { // Increment
				*sourceAddress += 4;
			} else if (control->srcControl == 1) { // Decrement
				*sourceAddress -= 4;
			}

			hasTransferred = true;
		}
	} else { // 16 bit
		for (int i = 0; i < length; i++) {
			if (*sourceAddress < 0x2000000) {
				bus.write<u16>(*destinationAddress & ~1, (u16)*openBus, hasTransferred);
			} else {
				u16 data = bus.read<u16, false>(*sourceAddress & ~1, hasTransferred);
				if (!hasTransferred && (*sourceAddress >= 0x8000000) && (*destinationAddress >= 0x8000000))
					hasTransferred = true;
				bus.write<u16>(*destinationAddress & ~1, data, hasTransferred);
				*openBus = (data << 16) | data;
			}

			if ((control->dstControl == 0) || (control->dstControl == 3)) { // Increment
				*destinationAddress += 2;
			} else if (control->dstControl == 1) { // Decrement
				*destinationAddress -= 2;
			}

			if (control->srcControl == 0) { // Increment
				*sourceAddress += 2;
			} else if (control->srcControl == 1) { // Decrement
				*sourceAddress -= 2;
			}

			hasTransferred = true;
		}
	}
	*destinationAddress &= 0x07FFFFFF;
	*sourceAddress &= 0x0FFFFFFF;

	dmaEnd();
	bus.internalCycle(1);
}

void GBADMA::dmaEnd() {
	switch (currentDma) {
	case 0:
		if (internalDMA0CNT.dstControl == 3)
			internalDMA0DAD = DMA0DAD;

		if (internalDMA0CNT.requestInterrupt)
			bus.cpu.requestInterrupt(GBACPU::IRQ_DMA0);

		if (!internalDMA0CNT.repeat)
			DMA0CNT.enable = false;
		internalDMA0CNT = DMA0CNT;
		break;
	case 1:
		if (internalDMA1CNT.dstControl == 3)
			internalDMA1DAD = DMA1DAD;

		if (internalDMA1CNT.requestInterrupt)
			bus.cpu.requestInterrupt(GBACPU::IRQ_DMA1);

		if (!internalDMA1CNT.repeat)
			DMA1CNT.enable = false;
		internalDMA1CNT = DMA1CNT;
		break;
	case 2:
		if (internalDMA2CNT.dstControl == 3)
			internalDMA2DAD = DMA2DAD;

		if (internalDMA2CNT.requestInterrupt)
			bus.cpu.requestInterrupt(GBACPU::IRQ_DMA2);

		if (!internalDMA2CNT.repeat)
			DMA2CNT.enable = false;
		internalDMA2CNT = DMA2CNT;
		break;
	case 3:
		if (internalDMA3CNT.dstControl == 3)
			internalDMA3DAD = DMA3DAD;

		if (internalDMA3CNT.requestInterrupt)
			bus.cpu.requestInterrupt(GBACPU::IRQ_DMA3);

		if (!internalDMA3CNT.repeat)
			DMA3CNT.enable = false;
		internalDMA3CNT = DMA3CNT;
		break;
	}

	currentDma = -1;
	checkDma();
}

u8 GBADMA::readIO(u32 address) {
	switch (address) {
	case 0x40000B8:
		return 0;
	case 0x40000B9:
		return 0;
	case 0x40000BA:
		return (u8)(DMA0CNT.raw >> 16);
	case 0x40000BB:
		return (u8)(DMA0CNT.raw >> 24);
	case 0x40000C4:
		return 0;
	case 0x40000C5:
		return 0;
	case 0x40000C6:
		return (u8)(DMA1CNT.raw >> 16);
	case 0x40000C7:
		return (u8)(DMA1CNT.raw >> 24);
	case 0x40000D0:
		return 0;
	case 0x40000D1:
		return 0;
	case 0x40000D2:
		return (u8)(DMA2CNT.raw >> 16);
	case 0x40000D3:
		return (u8)(DMA2CNT.raw >> 24);
	case 0x40000DC:
		return 0;
	case 0x40000DD:
		return 0;
	case 0x40000DE:
		return (u8)(DMA3CNT.raw >> 16);
	case 0x40000DF:
		return (u8)(DMA3CNT.raw >> 24);
	default:
		return bus.openBus<u8>(address);
	}
}

void GBADMA::writeIO(u32 address, u8 value) {
	bool oldEnable;

	switch (address) {
	case 0x40000B0:
		DMA0SAD = (DMA0SAD & 0xFFFFFF00) | value;
		break;
	case 0x40000B1:
		DMA0SAD = (DMA0SAD & 0xFFFF00FF) | (value << 8);
		break;
	case 0x40000B2:
		DMA0SAD = (DMA0SAD & 0xFF00FFFF) | (value << 16);
		break;
	case 0x40000B3:
		DMA0SAD = (DMA0SAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x40000B4:
		DMA0DAD = (DMA0DAD & 0xFFFFFF00) | value;
		break;
	case 0x40000B5:
		DMA0DAD = (DMA0DAD & 0xFFFF00FF) | (value << 8);
		break;
	case 0x40000B6:
		DMA0DAD = (DMA0DAD & 0xFF00FFFF) | (value << 16);
		break;
	case 0x40000B7:
		DMA0DAD = (DMA0DAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x40000B8:
		DMA0CNT.raw = (DMA0CNT.raw & 0xFFFFFF00) | value;
		break;
	case 0x40000B9:
		DMA0CNT.raw = (DMA0CNT.raw & 0xFFFF00FF) | ((value & 0x3F) << 8);
		break;
	case 0x40000BA:
		DMA0CNT.raw = (DMA0CNT.raw & 0xFF00FFFF) | ((value & 0xE0) << 16);
		break;
	case 0x40000BB:
		oldEnable = DMA0CNT.enable;
		DMA0CNT.raw = (DMA0CNT.raw & 0x00FFFFFF) | ((value & 0xF7) << 24);

		if ((value & 0x80) && !oldEnable) {
			internalDMA0SAD = DMA0SAD;
			internalDMA0DAD = DMA0DAD;
			internalDMA0CNT = DMA0CNT;

			if (internalDMA0CNT.timing == 0) {
				dma0Queued = true;
				//checkDma();
				bus.cpu.addEvent(2, dmaCheckEvent, this); // TODO: Check how long this is and when it happens
			}
		}
		break;
	case 0x40000BC:
		DMA1SAD = (DMA1SAD & 0xFFFFFF00) | value;
		break;
	case 0x40000BD:
		DMA1SAD = (DMA1SAD & 0xFFFF00FF) | (value << 8);
		break;
	case 0x40000BE:
		DMA1SAD = (DMA1SAD & 0xFF00FFFF) | (value << 16);
		break;
	case 0x40000BF:
		DMA1SAD = (DMA1SAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000C0:
		DMA1DAD = (DMA1DAD & 0xFFFFFF00) | value;
		break;
	case 0x40000C1:
		DMA1DAD = (DMA1DAD & 0xFFFF00FF) | (value << 8);
		break;
	case 0x40000C2:
		DMA1DAD = (DMA1DAD & 0xFF00FFFF) | (value << 16);
		break;
	case 0x40000C3:
		DMA1DAD = (DMA1DAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x40000C4:
		DMA1CNT.raw = (DMA1CNT.raw & 0xFFFFFF00) | value;
		break;
	case 0x40000C5:
		DMA1CNT.raw = (DMA1CNT.raw & 0xFFFF00FF) | ((value & 0x3F) << 8);
		break;
	case 0x40000C6:
		DMA1CNT.raw = (DMA1CNT.raw & 0xFF00FFFF) | ((value & 0xE0) << 16);
		break;
	case 0x40000C7:
		oldEnable = DMA1CNT.enable;
		DMA1CNT.raw = (DMA1CNT.raw & 0x00FFFFFF) | ((value & 0xF7) << 24);

		if ((value & 0x80) && !oldEnable) {
			internalDMA1SAD = DMA1SAD;
			internalDMA1DAD = DMA1DAD;
			internalDMA1CNT = DMA1CNT;

			if (internalDMA1CNT.timing == 0) {
				dma1Queued = true;
				//checkDma();
				bus.cpu.addEvent(2, dmaCheckEvent, this);
			}
		}
		break;
	case 0x40000C8:
		DMA2SAD = (DMA2SAD & 0xFFFFFF00) | value;
		break;
	case 0x40000C9:
		DMA2SAD = (DMA2SAD & 0xFFFF00FF) | (value << 8);
		break;
	case 0x40000CA:
		DMA2SAD = (DMA2SAD & 0xFF00FFFF) | (value << 16);
		break;
	case 0x40000CB:
		DMA2SAD = (DMA2SAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000CC:
		DMA2DAD = (DMA2DAD & 0xFFFFFF00) | value;
		break;
	case 0x40000CD:
		DMA2DAD = (DMA2DAD & 0xFFFF00FF) | (value << 8);
		break;
	case 0x40000CE:
		DMA2DAD = (DMA2DAD & 0xFF00FFFF) | (value << 16);
		break;
	case 0x40000CF:
		DMA2DAD = (DMA2DAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x40000D0:
		DMA2CNT.raw = (DMA2CNT.raw & 0xFFFFFF00) | value;
		break;
	case 0x40000D1:
		DMA2CNT.raw = (DMA2CNT.raw & 0xFFFF00FF) | ((value & 0x3F) << 8);
		break;
	case 0x40000D2:
		DMA2CNT.raw = (DMA2CNT.raw & 0xFF00FFFF) | ((value & 0xE0) << 16);
		break;
	case 0x40000D3:
		oldEnable = DMA2CNT.enable;
		DMA2CNT.raw = (DMA2CNT.raw & 0x00FFFFFF) | ((value & 0xF7) << 24);

		if ((value & 0x80) && !oldEnable) {
			internalDMA2SAD = DMA2SAD;
			internalDMA2DAD = DMA2DAD;
			internalDMA2CNT = DMA2CNT;

			if (internalDMA2CNT.timing == 0) {
				dma2Queued = true;
				//checkDma();
				bus.cpu.addEvent(2, dmaCheckEvent, this);
			}
		}
		break;
	case 0x40000D4:
		DMA3SAD = (DMA3SAD & 0xFFFFFF00) | value;
		break;
	case 0x40000D5:
		DMA3SAD = (DMA3SAD & 0xFFFF00FF) | (value << 8);
		break;
	case 0x40000D6:
		DMA3SAD = (DMA3SAD & 0xFF00FFFF) | (value << 16);
		break;
	case 0x40000D7:
		DMA3SAD = (DMA3SAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000D8:
		DMA3DAD = (DMA3DAD & 0xFFFFFF00) | value;
		break;
	case 0x40000D9:
		DMA3DAD = (DMA3DAD & 0xFFFF00FF) | (value << 8);
		break;
	case 0x40000DA:
		DMA3DAD = (DMA3DAD & 0xFF00FFFF) | (value << 16);
		break;
	case 0x40000DB:
		DMA3DAD = (DMA3DAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000DC:
		DMA3CNT.raw = (DMA3CNT.raw & 0xFFFFFF00) | value;
		break;
	case 0x40000DD:
		DMA3CNT.raw = (DMA3CNT.raw & 0xFFFF00FF) | (value << 8);
		break;
	case 0x40000DE:
		DMA3CNT.raw = (DMA3CNT.raw & 0xFF00FFFF) | ((value & 0xE0) << 16);
		break;
	case 0x40000DF:
		oldEnable = DMA3CNT.enable;
		DMA3CNT.raw = (DMA3CNT.raw & 0x00FFFFFF) | (value << 24);

		if ((value & 0x80) && !oldEnable) {
			internalDMA3SAD = DMA3SAD;
			internalDMA3DAD = DMA3DAD;
			internalDMA3CNT = DMA3CNT;

			if (internalDMA3CNT.timing == 0) {
				dma3Queued = true;
				//checkDma();
				bus.cpu.addEvent(2, dmaCheckEvent, this);
			}
		}
		break;
	}
}