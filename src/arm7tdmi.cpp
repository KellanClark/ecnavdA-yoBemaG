
#include "typedefs.hpp"
#include "gba.hpp"

ARM7TDMI::ARM7TDMI(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void ARM7TDMI::reset() {
	pipelineStage = 0;
	reg.R[15] = 0x08000000; // Start of ROM
}

void ARM7TDMI::cycle() {
	pipelineOpcode3 = pipelineOpcode2;
	pipelineOpcode2 = pipelineOpcode1;
	pipelineOpcode1 = reg.thumbMode ? bus.read<u16>(reg.R[15]) : bus.read<u32>(reg.R[15]);

	incrementR15 = true;
	if (pipelineStage == 3) {
		if (checkCondition(pipelineOpcode3 >> 28)) {
			u32 lutIndex = ((pipelineOpcode3 & 0x0FF00000) >> 16) | ((pipelineOpcode3 & 0x000000F0) >> 4);
			(this->*LUT[lutIndex])(pipelineOpcode3);
		}
	} else {
		++pipelineStage;
	}
	if (incrementR15)
		reg.R[15] += reg.thumbMode ? 2 : 4;
	
	(this->*LUT[0])(0);
	(this->*LUT[1])(0);
}

/*template <bool word>
void ARM7TDMI::helloWorld(u32 opcode) {
	if (word) {
		printf("World!\n");
	} else {
		printf("Hello ");
	}
}*/

/* Instruction Decoding/Executing */
void ARM7TDMI::unknownOpcodeArm(u32 opcode) {
	bus.running = false;
	printf("Unknown opcode 0x%08X at address 0x%08X\n", opcode, reg.R[15] - 8);
}

bool ARM7TDMI::checkCondition(int condtionCode) {
	switch (condtionCode) {
	case 0x0: return reg.flagZ;
	case 0x1: return !reg.flagZ;
	case 0x2: return reg.flagC;
	case 0x3: return !reg.flagC;
	case 0x4: return reg.flagN;
	case 0x5: return !reg.flagN;
	case 0x6: return reg.flagV;
	case 0x7: return !reg.flagV;
	case 0x8: return reg.flagC && !reg.flagZ;
	case 0x9: return !reg.flagC || reg.flagZ;
	case 0xA: return reg.flagN == reg.flagV;
	case 0xB: return reg.flagN != reg.flagV;
	case 0xC: return !reg.flagZ && (reg.flagN == reg.flagV);
	case 0xD: return reg.flagZ || (reg.flagN != reg.flagV);
	case 0xE: return true;
	default:
		unknownOpcodeArm(pipelineOpcode3);
		return false;
	}
}

// Fill a Look Up Table with entries for the combinations of bits 27-20 and 7-4
template<size_t i>
void constexpr recursive_loop(std::array<void (ARM7TDMI::*)(u32), 4096>& inLut) {
	inLut[i] = &ARM7TDMI::unknownOpcodeArm;
	/*if (i < 2) {
		inLut[i] = &ARM7TDMI::helloWorld<i & 1>;
	}*/

	//inlut[i] = &ARM7TDMI::whatever_function_pointer;

	if constexpr (i < 4096)
		recursive_loop<i+1>(inLut);
}

const std::array<void (ARM7TDMI::*)(u32), 4096> ARM7TDMI::LUT = []()constexpr {
	std::array<void (ARM7TDMI::*)(u32), 4096> tmpLut = {};
	recursive_loop<0>(tmpLut);
	return tmpLut;
}();