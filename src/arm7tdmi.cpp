
#include "typedefs.hpp"
#include "arm7tdmi.hpp"

ARM7TDMI::ARM7TDMI(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void ARM7TDMI::reset() {
	//
}

void ARM7TDMI::cycle() {
	//
	(this->*LUT[0])(0);
	(this->*LUT[1])(0);
}

void ARM7TDMI::helloFunc(u32 opcode) {
	printf("Hello ");
}

void ARM7TDMI::worldFunc(u32 opcode) {
	printf("World!\n");
}

// Fill a Look Up Table with entries for the combinations of bits 27-20 and 7-4
template<size_t i>
void constexpr recursive_loop(std::array<void (ARM7TDMI::*)(u32), 4096>& inLut) {
	if constexpr (i == 0) {
		inLut[i] = &ARM7TDMI::helloFunc;
	} else if constexpr (i == 1) {
		inLut[i] = &ARM7TDMI::worldFunc;
	}
	//inlut[i] = whatever_function_pointer;

	if constexpr (i < 4096)
		recursive_loop<i+1>(inLut);
}

const std::array<void (ARM7TDMI::*)(u32), 4096> ARM7TDMI::LUT = []()constexpr {
	std::array<void (ARM7TDMI::*)(u32), 4096> tmpLut = {};
	recursive_loop<0>(tmpLut);
	return tmpLut;
}();