
#include "timer.hpp"
#include "scheduler.hpp"

GBATIMER::GBATIMER(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void GBATIMER::reset() {
	//
}

u8 GBATIMER::readIO(u32 address) {
	switch (address) {
	default:
		return 0;
	}
}

void GBATIMER::writeIO(u32 address, u8 value) {
	switch (address) {
	//
	}
}