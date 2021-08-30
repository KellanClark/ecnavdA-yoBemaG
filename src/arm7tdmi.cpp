
#include "gba.hpp"

ARM7TDMI::ARM7TDMI(GameBoyAdvance& bus_) : bus(bus_) {
	disassemblerOptions.showALCondition = false;
	disassemblerOptions.alwaysShowSBit = false;
	disassemblerOptions.printOperandsHex = false;
	disassemblerOptions.printAddressesHex = true;

	reset();
}

void ARM7TDMI::reset() {
	pipelineStage = 0;
	incrementR15 = false;

	for (int i = 0; i < 15; i++)
		reg.R[i] = 0;
	reg.R[15] = 0x08000000; // Start of ROM

	reg.CPSR = 0x10;
	reg.SPSR = 0;

	reg.R8_fiq = reg.R9_fiq = reg.R10_fiq = reg.R11_fiq = reg.R12_fiq = reg.R13_fiq = reg.R14_fiq = reg.SPSR_fiq = 0;
	reg.R13_svc = reg.R14_svc = reg.SPSR_svc = 0;
	reg.R13_abt = reg.R14_abt = reg.SPSR_abt = 0;
	reg.R13_irq = reg.R14_irq = reg.SPSR_irq = 0;
	reg.R13_und = reg.R14_und = reg.SPSR_und = 0;
}

void ARM7TDMI::cycle() {
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
	incrementR15 = true;

	pipelineOpcode3 = pipelineOpcode2;
	pipelineOpcode2 = pipelineOpcode1;
	pipelineOpcode1 = bus.read<u32>(reg.R[15]);
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
		unknownOpcodeArm(pipelineOpcode3, "Invalid condition");
		return false;
	}
}

/* Instruction Decoding/Executing */
static const u32 armDataProcessingMask = 0b1100'0000'0000;
static const u32 armDataProcessingBits = 0b0000'0000'0000;
static const u32 armMultiplyMask = 0b1111'1100'1111;
static const u32 armMultiplyBits = 0b0000'0000'1001;
static const u32 armMultiplyLongMask = 0b1111'1000'1111;
static const u32 armMultiplyLongBits = 0b0000'1000'1001;
static const u32 armSingleDataSwapMask = 0b1111'1011'1111;
static const u32 armSingleDataSwapBits = 0b0001'0000'1001;
static const u32 armBranchExchangeMask = 0b1111'1111'1111;
static const u32 armBranchExchangeBits = 0b0001'0010'0001;
static const u32 armHalfwordDataTransferMask = 0b1110'0000'1001;
static const u32 armHalfwordDataTransferBits = 0b0000'0000'1001;
static const u32 armSingleDataTransferMask = 0b1100'0000'0000;
static const u32 armSingleDataTransferBits = 0b0100'0000'0000;
static const u32 armUndefinedMask = 0b1110'0000'0001;
static const u32 armUndefinedBits = 0b0110'0000'0001;
static const u32 armBlockDataTransferMask = 0b1110'0000'0000;
static const u32 armBlockDataTransferBits = 0b1000'0000'0000;
static const u32 armBranchMask = 0b1110'0000'0000;
static const u32 armBranchBits = 0b1010'0000'0000;
static const u32 armSoftwareInterruptMask = 0b1111'0000'0000;
static const u32 armSoftwareInterruptBits = 0b1111'0000'0000;

std::string ARM7TDMI::disassemble(u32 address, u32 opcode) {
	std::stringstream disassembledOpcode;

	// Get condition code
	std::string condtionCode;
	switch (opcode >> 28) {
	case 0x0: condtionCode = "EQ"; break;
	case 0x1: condtionCode = "NE"; break;
	case 0x2: condtionCode = "CS"; break;
	case 0x3: condtionCode = "CC"; break;
	case 0x4: condtionCode = "MI"; break;
	case 0x5: condtionCode = "PL"; break;
	case 0x6: condtionCode = "VS"; break;
	case 0x7: condtionCode = "VC"; break;
	case 0x8: condtionCode = "HI"; break;
	case 0x9: condtionCode = "LS"; break;
	case 0xA: condtionCode = "GE"; break;
	case 0xB: condtionCode = "LT"; break;
	case 0xC: condtionCode = "GT"; break;
	case 0xD: condtionCode = "LE"; break;
	case 0xE: condtionCode = disassemblerOptions.showALCondition ? "AL" : ""; break;
	default: return "Undefined";
	}

	u32 lutIndex = ((opcode & 0x0FF00000) >> 16) | ((opcode & 0x000000F0) >> 4);
	if ((lutIndex & armBranchMask) == armBranchBits) {
		if (lutIndex & 0b0001'0000'0000) {
			disassembledOpcode << "BL";
		} else {
			disassembledOpcode << "B";
		}
		disassembledOpcode << condtionCode;

		u32 jumpLocation = address + (((i32)((opcode & 0x00FFFFFF) << 8)) >> 6) + 8;
		if (disassemblerOptions.printAddressesHex) {
			disassembledOpcode << " $" << std::hex << jumpLocation;
		} else {
			disassembledOpcode << " #" << jumpLocation;
		}

		return disassembledOpcode.str();
	} else if ((lutIndex & armDataProcessingMask) == armDataProcessingBits) {
		bool iBit = lutIndex & 0b0010'0000'0000;
		auto operation = (lutIndex & 0b0001'1110'0000) >> 5;
		bool sBit = lutIndex & 0b0000'0001'0000;

		bool printRd = true;
		bool printRn = true;
		switch (operation) {
		case 0x0: disassembledOpcode << "AND"; break;
		case 0x1: disassembledOpcode << "EOR"; break;
		case 0x2: disassembledOpcode << "SUB"; break;
		case 0x3: disassembledOpcode << "RSB"; break;
		case 0x4: disassembledOpcode << "ADD"; break;
		case 0x5: disassembledOpcode << "ADC"; break;
		case 0x6: disassembledOpcode << "SBC"; break;
		case 0x7: disassembledOpcode << "RSC"; break;
		case 0x8: disassembledOpcode << "TST"; printRd = false; break;
		case 0x9: disassembledOpcode << "TEQ"; printRd = false; break;
		case 0xA: disassembledOpcode << "CMP"; printRd = false; break;
		case 0xB: disassembledOpcode << "CMN"; printRd = false; break;
		case 0xC: disassembledOpcode << "ORR"; break;
		case 0xD: disassembledOpcode << "MOV"; printRn = false; break;
		case 0xE: disassembledOpcode << "BIC"; break;
		case 0xF: disassembledOpcode << "MVN"; printRn = false; break;
		}
		
		if ((printRd && sBit) || disassemblerOptions.alwaysShowSBit)
			disassembledOpcode << "S";
		disassembledOpcode << condtionCode << " ";
		if (printRd)
			disassembledOpcode << "r" << (((0xF << 12) & opcode) >> 12) << ", ";
		if (printRn)
			disassembledOpcode << "r" << (((0xF << 16) & opcode) >> 16) << ", ";

		if (iBit) {
			u32 shiftAmount = (opcode & (0xF << 8)) >> 7;
			u32 shiftInput = opcode & 0xFF;
			shiftInput = shiftAmount ? ((shiftInput >> shiftAmount) | (shiftInput << (32 - shiftAmount))) : shiftInput;
			if (disassemblerOptions.printOperandsHex) {
				disassembledOpcode << "$" << std::hex << shiftInput;
			} else {
				disassembledOpcode << "#" << shiftInput;
			}
		} else {
			disassembledOpcode << "r" << (opcode & 0xF) << ", ";
			
			switch ((opcode & (3 << 5)) >> 5) {
			case 0:
				disassembledOpcode << "LSL ";
				break;
			case 1:
				disassembledOpcode << "LSR ";
				break;
			case 2:
				disassembledOpcode << "ASR ";
				break;
			case 3:
				if (!(opcode & (1 << 4)) && (((opcode & (0x1F << 7)) >> 7) == 0)) {
					disassembledOpcode << "RRX ";
				} else {
					disassembledOpcode << "ROR ";
				}
				break;
			}

			if (opcode & (1 << 4)) {
				disassembledOpcode << "r" << ((opcode & (0xF << 8)) >> 8);
			} else {
				auto shiftAmount = ((opcode & (0x1F << 7)) >> 7);
				if ((shiftAmount == 0) && ((opcode & (3 << 5)) != 0))
					shiftAmount = 32;

				if (disassemblerOptions.printOperandsHex) {
					disassembledOpcode << "$" << std::hex << shiftAmount;
				} else {
					disassembledOpcode << "#" << shiftAmount;
				}
			}
		}

		return disassembledOpcode.str();
	}

	return "Undefined";
}

void ARM7TDMI::unknownOpcodeArm(u32 opcode) {
	unknownOpcodeArm(opcode, "No LUT entry");
}

void ARM7TDMI::unknownOpcodeArm(u32 opcode, std::string message) {
	bus.running = false;
	bus.log << fmt::format("Unknown opcode 0x{:0>8X} at address 0x{:0>8X}  Message: {}\n", opcode, reg.R[15] - 8, message.c_str());
}

template <bool iBit, int operation, bool sBit>
void ARM7TDMI::dataProcessing(u32 opcode) {
	// Shift and rotate to get operands
	bool shifterCarry = 0;
	u32 operand1 = reg.R[(opcode >> 16) & 0xF];
	u32 operand2;
	int shiftAmount = 0;
	if (iBit) {
		operand2 = opcode & 0xFF;
		shiftAmount = (opcode & (0xF << 8)) >> 7;
		if (shiftAmount == 0) {
			shifterCarry = (operand2 & (1 << 31)) > 0;
		} else {
			shifterCarry = operand2 & (1 << (shiftAmount - 1));
			operand2 = (operand2 >> shiftAmount) | (operand2 << (32 - shiftAmount));
		}
	} else {
		operand2 = reg.R[opcode & 0xF];
		shiftAmount = (opcode & (1 << 4)) ? (reg.R[(opcode & (0xF << 8)) >> 8] & 0xFF) : ((opcode & (0x1F << 7)) >> 7);
		switch ((opcode & (3 << 5)) >> 5) {
		case 0: // LSL
			if (shiftAmount != 0) {
				if (shiftAmount > 31) {
					shifterCarry = (shiftAmount == 32) ? (operand2 & 1) : 0;
					operand2 = 0;
					break;
				}
				shifterCarry = operand2 & (1 << (31 - (shiftAmount - 1)));
				operand2 <<= shiftAmount;
			} else {
				shifterCarry = reg.flagC;
			}
			break;
		case 3: // ROR
			if (opcode & (1 << 4)) { // Using register as shift amount
				if (shiftAmount == 0) {
					shifterCarry = reg.flagC;
					break;
				}
				shiftAmount &= 31;
				if (shiftAmount == 0) {
					shifterCarry = operand2 >> 31;
					break;
				}
			} else {
				if (shiftAmount == 0) { // RRX
					shifterCarry = operand2 & 1;
					operand2 = (operand2 >> 1) | (reg.flagC << 31);
					break;
				}
			}
			shifterCarry = operand2 & (1 << (shiftAmount - 1));
			operand2 = (operand2 >> shiftAmount) | (operand2 << (32 - shiftAmount));
			break;
		default:
			unknownOpcodeArm(opcode, "Shift type");
			return;
		}
	}

	// Perform operation
	bool operationCarry = reg.flagC;
	bool operationOverflow = reg.flagV;
	u32 result = 0;
	auto destinationReg = (opcode & (0xF << 12)) >> 12;
	if (destinationReg == 15)
		unknownOpcodeArm(opcode, "r15 as destination");
	switch (operation) {
	case 0x4: // ADD
		operationOverflow = ((operand1 & 0x7FFFFFFF) + (operand2 & 0x7FFFFFFF)) > 0x7FFFFFFF;
		operationCarry = (((u64)operand1) + ((u64)operand2)) > 0xFFFFFFFF;
		result = operand1 + operand2;
		break;
	case 0xD: // MOV
		result = operand2;
		break;
	case 0xF: // MVN
		result = ~operand2;
		break;
	default:
		unknownOpcodeArm(opcode, "Operation");
		return;
	}
	reg.R[destinationReg] = result;

	// Compute common flags
	if (sBit) {
		reg.flagN = (bool)(result & (1 << 31)); 
		reg.flagZ = result == 0;
		if ((operation < 2) || (operation == 8) || (operation == 9) || (operation >= 0xC)) { // Logical operations
			reg.flagC = shifterCarry;
		} else {
			reg.flagC = operationCarry;
			reg.flagV = operationOverflow;
		}
	}
}

template <bool prePostIndex, bool upDown, bool immediateOffset, bool writeBack, bool loadStore, bool sBit, bool hBit>
void ARM7TDMI::halfwordDataTransfer(u32 opcode) {
	//

	unknownOpcodeArm(opcode);
}

template <bool lBit>
void ARM7TDMI::branch(u32 opcode) {
	if (lBit)
		reg.R[14] = reg.R[15] - 4;
	reg.R[15] += ((i32)((opcode & 0x00FFFFFF) << 8)) >> 6;

	pipelineStage = 1;
	incrementR15 = false;
}

using lutEntry = void (ARM7TDMI::*)(u32);

template <std::size_t lutFillIndex>
constexpr lutEntry decode() {
	if ((lutFillIndex & armBranchMask) == armBranchBits) {
		return &ARM7TDMI::branch<(bool)(lutFillIndex & 0b0001'0000'0000)>;
	} else if ((lutFillIndex & armHalfwordDataTransferMask) == armHalfwordDataTransferBits) {
		return &ARM7TDMI::halfwordDataTransfer<(bool)(lutFillIndex & 0b0001'0000'0000), (bool)(lutFillIndex & 0b0000'1000'0000), (bool)(lutFillIndex & 0b0000'0100'0000), (bool)(lutFillIndex & 0b0000'0010'0000), (bool)(lutFillIndex & 0b0000'0001'0000), (bool)(lutFillIndex & 0b0000'0000'0100), (bool)(lutFillIndex & 0b0000'0000'0010)>;
	} else if ((lutFillIndex & armDataProcessingMask) == armDataProcessingBits) {
		return &ARM7TDMI::dataProcessing<(bool)(lutFillIndex & 0b0010'0000'0000), ((lutFillIndex & 0b0001'1110'0000) >> 5), (bool)(lutFillIndex & 0b0000'0001'0000)>;
	}

	return &ARM7TDMI::unknownOpcodeArm;
}

template <std::size_t... lutFillIndex>
constexpr std::array<lutEntry, 4096> generate_table(std::index_sequence<lutFillIndex...>) {
    return std::array { decode<lutFillIndex>()... };
}

constexpr std::array<lutEntry, 4096> ARM7TDMI::LUT = {
    generate_table(std::make_index_sequence<4096>())
};