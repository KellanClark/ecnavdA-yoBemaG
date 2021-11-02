
#include "apu.hpp"
#include "scheduler.hpp"

GBAAPU::GBAAPU(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void GBAAPU::reset() {
	//
}

u8 GBAAPU::readIO(u32 address) {
	switch (address) {
	default:
		return 0;
	}
}

void GBAAPU::writeIO(u32 address, u8 value) {
	switch (address) {
	//
	}
}