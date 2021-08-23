
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
	pipelineOpcode1 = bus.read<u32>(reg.R[15]);

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
		reg.R[15] += 4;

	//(this->*LUT[0])(0);
	//(this->*LUT[1])(0);
}

template <bool word>
void ARM7TDMI::helloWorld(u32 opcode) {
	if (word) {
		printf("World!\n");
	} else {
		printf("Hello ");
	}
}

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
/*template<size_t i>
void constexpr recursive_loop(std::array<void (ARM7TDMI::*)(u32), 4096>& inLut) {
	inLut[i] = &ARM7TDMI::unknownOpcodeArm;
	if (i < 2) {
		inLut[i] = &ARM7TDMI::helloWorld<i & 1>;
	}

	inlut[i] = &ARM7TDMI::whatever_function_pointer;

	if constexpr (i < 4095)
		recursive_loop<i+1>(inLut);
}

constexpr std::array<void (ARM7TDMI::*)(u32), 4096> ARM7TDMI::LUT = []()constexpr {
	std::array<void (ARM7TDMI::*)(u32), 4096> tmpLut = {};
	recursive_loop<0>(tmpLut);
	return tmpLut;
}();*/

static const u32 dataProcessingMask = 0b1100'0000'0000;
const u32 dataProcessingBits = 0b0000'0000'0000;
const u32 multiplyMask = 0b1111'1100'1111;
const u32 multiplyBits = 0b0000'0000'1001;
const u32 multiplyLongMask = 0b1111'1000'1111;
const u32 multiplyLongBits = 0b0000'1000'1001;
const u32 singleDataSwapMask = 0b1111'1011'1111;
const u32 singleDataSwapBits = 0b0001'0000'1001;
const u32 branchExchangeMask = 0b1111'1111'1111;
const u32 branchExchangeBits = 0b0001'0010'0001;
const u32 hwdtRegisterOffsetMask = 0b1110'0100'1001;
const u32 hwdtRegisterOffsetBits = 0b0000'0000'1001;
const u32 hwdtImmediateOffsetMask = 0b1110'0100'1001;
const u32 hwdtImmediateOffsetBits = 0b0000'0100'1001;
const u32 singleDataTransferMask = 0b1100'0000'0000;
const u32 singleDataTransferBits = 0b0100'0000'0000;
const u32 undefinedMask = 0b1110'0000'0001;
const u32 undefinedBits = 0b0110'0000'0001;
const u32 blockDataTransferMask = 0b1110'0000'0000;
const u32 blockDataTransferBits = 0b1000'0000'0000;
const u32 branchMask = 0b1110'0000'0000;
const u32 branchBits = 0b1010'0000'0000;
const u32 softwareInterruptMask = 0b1111'0000'0000;
const u32 softwareInterruptBits = 0b1111'0000'0000;

using lutEntry = void (ARM7TDMI::*)(u32);

template <std::size_t lutFillIndex>
constexpr lutEntry decode() {
	return &ARM7TDMI::unknownOpcodeArm;
}

template <std::size_t... lutFillIndex>
constexpr std::array<lutEntry, 4096> generate_table(std::index_sequence<lutFillIndex...>) {
    return std::array { decode<lutFillIndex>()... };
}

constexpr std::array<lutEntry, 4096> ARM7TDMI::LUT = {
    generate_table(std::make_index_sequence<4096>())
};