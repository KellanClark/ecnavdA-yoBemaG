
#include "arm7tdmi.hpp"
#include "gba.hpp"
#include <bit>
#include <cstdio>

ARM7TDMI::ARM7TDMI(GameBoyAdvance& bus_) : bus(bus_) {
	disassemblerOptions.showALCondition = false;
	disassemblerOptions.alwaysShowSBit = false;
	disassemblerOptions.printOperandsHex = true;
	disassemblerOptions.printAddressesHex = true;
	disassemblerOptions.simplifyRegisterNames = false;
	disassemblerOptions.simplifyPushPop = false;
	disassemblerOptions.ldmStmStackSuffixes = false;

	resetARM7TDMI();
}

void ARM7TDMI::resetARM7TDMI() {
	pipelineStage = 0;
	incrementR15 = false;

	reg.R[0] = 0x08000000;
	reg.R[1] = 0x000000EA;
	reg.R[2] = 0x00000000;
	reg.R[3] = 0x00000000;
	reg.R[4] = 0x00000000;
	reg.R[5] = 0x00000000;
	reg.R[6] = 0x00000000;
	reg.R[7] = 0x00000000;
	reg.R[8] = 0x00000000;
	reg.R[9] = 0x00000000;
	reg.R[10] = 0x00000000;
	reg.R[11] = 0x00000000;
	reg.R[12] = 0x00000000;
	reg.R[13] = 0x03007F00;
	reg.R[14] = 0x00000000;
	reg.R[15] = 0x08000000; // Start of ROM

	reg.CPSR = 0x6000001F;
	reg.SPSR = 0;

	reg.R8_user = reg.R9_user = reg.R10_user = reg.R11_user = reg.R12_user = reg.R13_user = reg.R14_user = 0;
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

	//if (reg.R[15] == 0x08001EE4)
	//	unknownOpcodeArm(pipelineOpcode3, "BKPT");
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
	if ((lutIndex & armHalfwordDataTransferMask) == armHalfwordDataTransferBits) {
		bool prePostIndex = lutIndex & 0b0001'0000'0000;
		bool upDown = lutIndex & 0b0000'1000'0000;
		bool immediateOffset = lutIndex & 0b0000'0100'0000;
		bool writeBack = lutIndex & 0b0000'0010'0000;
		bool loadStore = lutIndex & 0b0000'0001'0000;
		int shBits = (lutIndex & 0b0000'0000'0110) >> 1;

		disassembledOpcode << (loadStore ? "LDR" : "STR") << condtionCode;
		switch (shBits) {
		case 0:
			return "Undefined";
		case 1:
			disassembledOpcode << "H";
			break;
		case 2:
			disassembledOpcode << "SB";
			break;
		case 3:
			disassembledOpcode << "SH";
			break;
		}

		disassembledOpcode << " r" << ((opcode >> 12) & 0xF) << ", [r" << ((opcode >> 16) & 0xF);

		u32 offset;
		if (immediateOffset) {
			offset = ((opcode & 0xF00) >> 4) | (opcode & 0xF);
			if (offset == 0) {
				disassembledOpcode << "]";
				return disassembledOpcode.str();
			}
		}

		if (!prePostIndex)
			disassembledOpcode << "]";
		disassembledOpcode << ", ";

		if (immediateOffset) {
			disassembledOpcode << (upDown ? "#" : "#-") << offset;
		} else {
			disassembledOpcode << (upDown ? "+r" : "-r") << (opcode & 0xF);
		}

		if (prePostIndex)
			disassembledOpcode << "]" << (writeBack ? "!" : "");

		return disassembledOpcode.str();
	} else if ((lutIndex & armDataProcessingMask) == armDataProcessingBits) {
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
		
		disassembledOpcode << disassembleShift(opcode, false);

		return disassembledOpcode.str();
	} else if ((lutIndex & armSingleDataTransferMask) == armSingleDataTransferBits) {
		bool immediateOffset = lutIndex & 0b0010'0000'0000;
		bool prePostIndex = lutIndex & 0b0001'0000'0000;
		bool byteWord = lutIndex & 0b0000'0100'0000;
		bool writeBack = lutIndex & 0b0000'0010'0000;
		bool loadStore = lutIndex & 0b0000'0001'0000;

		disassembledOpcode << (loadStore ? "LDR" : "STR") << condtionCode << (byteWord ? "B" : "");

		disassembledOpcode << " r" << ((opcode >> 12) & 0xF) << ", [r" << ((opcode >> 16) & 0xF);

		u32 offset;
		if (immediateOffset) {
			offset = ((opcode & 0xF00) >> 4) | (opcode & 0xF);
			if (offset == 0) {
				disassembledOpcode << "]";
				return disassembledOpcode.str();
			}
		}

		if (!prePostIndex)
			disassembledOpcode << "], ";
		disassembledOpcode << disassembleShift(opcode, true);

		if (prePostIndex)
			disassembledOpcode << "]" << (writeBack ? "!" : "");

		return disassembledOpcode.str();
	} else if ((lutIndex & armBlockDataTransferMask) == armBlockDataTransferBits) {
		bool prePostIndex = lutIndex & 0b0001'0000'0000;
		bool upDown = lutIndex & 0b0000'1000'0000;
		bool sBit = lutIndex & 0b0000'0100'0000;
		bool writeBack = lutIndex & 0b0000'0010'0000;
		bool loadStore = lutIndex & 0b0000'0001'0000;
		auto baseRegister = (opcode >> 16) & 0xF;

		if (disassemblerOptions.simplifyPushPop && (baseRegister == 13) && ((loadStore && !prePostIndex && upDown) || (!loadStore && prePostIndex && !upDown)) && writeBack && !sBit) {
			disassembledOpcode << (loadStore ? "POP" : "PUSH") << condtionCode << " {";
		} else {
			disassembledOpcode << (loadStore ? "LDM" : "STM") << condtionCode;

			// Code based on Table 4-6: Addressing mode names
			if (disassemblerOptions.ldmStmStackSuffixes) {
				switch ((loadStore << 2) | (prePostIndex << 1) | upDown) {
				case 7: disassembledOpcode << "ED"; break;
				case 5: disassembledOpcode << "FD"; break;
				case 6: disassembledOpcode << "EA"; break;
				case 4: disassembledOpcode << "FA"; break;
				case 3: disassembledOpcode << "FA"; break;
				case 1: disassembledOpcode << "EA"; break;
				case 2: disassembledOpcode << "FD"; break;
				case 0: disassembledOpcode << "ED"; break;
				}
			} else {
				switch ((loadStore << 2) | (prePostIndex << 1) | upDown) {
				case 7: disassembledOpcode << "IB"; break;
				case 5: disassembledOpcode << "IA"; break;
				case 6: disassembledOpcode << "DB"; break;
				case 4: disassembledOpcode << "DA"; break;
				case 3: disassembledOpcode << "IB"; break;
				case 1: disassembledOpcode << "IA"; break;
				case 2: disassembledOpcode << "DB"; break;
				case 0: disassembledOpcode << "DA"; break;
				}
			}

			disassembledOpcode << " " << getRegName(baseRegister) << (writeBack ? "!" : "") << ", {";
		}

		// Print register list
		bool registerList[18] = {false};
		for (int i = 0; i < 16; i++)
			registerList[i] = (opcode & (1 << i));

		bool hasPrintedRegister = false;
		for (int i = 0; i < 16; i++) {
			if (registerList[i]) {
				disassembledOpcode << (hasPrintedRegister ? "," : "") << getRegName(i);
				hasPrintedRegister = true;

				if (registerList[i + 1] && registerList[i + 2]) { // Shorten group of registers
					disassembledOpcode << "-";
					do {
						++i;
					} while (registerList[i + 1]);
					disassembledOpcode << getRegName(i);
				}
			}
		}
		disassembledOpcode << "}";

		if (sBit)
			disassembledOpcode << "^";

		return disassembledOpcode.str();
	} else if ((lutIndex & armBranchMask) == armBranchBits) {
		if (lutIndex & 0b0001'0000'0000) {
			disassembledOpcode << "BL";
		} else {
			disassembledOpcode << "B";
		}
		disassembledOpcode << condtionCode;

		u32 jumpLocation = address + (((i32)((opcode & 0x00FFFFFF) << 8)) >> 6) + 8;
		if (disassemblerOptions.printAddressesHex) {
			disassembledOpcode << " #0x" << std::hex << jumpLocation;
		} else {
			disassembledOpcode << " #" << jumpLocation;
		}

		return disassembledOpcode.str();
	}

	return "Undefined";
}

std::string ARM7TDMI::getRegName(unsigned int regNumber) {
	if (disassemblerOptions.simplifyRegisterNames) {
		switch (regNumber) {
		case 13:
			return "sp";
		case 14:
			return "lr";
		case 15:
			return "pc";
		}
	}
	return (std::string)"r" + std::to_string(regNumber);
}

std::string ARM7TDMI::disassembleShift(u32 opcode, bool showUpDown) {
	std::stringstream returnValue;

	if (showUpDown && !((opcode >> 25) & 1)) {
		returnValue << ((showUpDown & !((opcode >> 23) & 1)) ? "#-" : "#");
		if (disassemblerOptions.printOperandsHex) {
			returnValue << "0x" << std::hex;
		}
		returnValue << (opcode & 0xFFF);
		
		return returnValue.str();
	}

	if (((opcode >> 25) & 1) && !showUpDown) {
		u32 shiftAmount = (opcode & (0xF << 8)) >> 7;
		u32 shiftInput = opcode & 0xFF;
		shiftInput = shiftAmount ? ((shiftInput >> shiftAmount) | (shiftInput << (32 - shiftAmount))) : shiftInput;
		
		if (disassemblerOptions.printOperandsHex) {
			returnValue << "#0x" << std::hex << shiftInput;
		} else {
			returnValue << "#" << shiftInput;
		}
	} else {
		if (showUpDown) {
			returnValue << (((opcode >> 23) & 1) ? "r" : "-r") << (opcode & 0xF);
		} else {
			returnValue << "r" << (opcode & 0xF);
		}

		if ((opcode & 0xFF0) == 0) // LSL #0
			return returnValue.str();
		
		switch ((opcode >> 5) & 3) {
		case 0:
			returnValue << ", LSL ";
			break;
		case 1:
			returnValue << ", LSR ";
			break;
		case 2:
			returnValue << ", ASR ";
			break;
		case 3:
			if (!(opcode & (1 << 4)) && (((opcode & (0x1F << 7)) >> 7) == 0)) {
				returnValue << ", RRX ";
			} else {
				returnValue << ", ROR ";
			}
			break;
		}

		if (opcode & (1 << 4)) {
			returnValue << "r" << ((opcode & (0xF << 8)) >> 8);
		} else {
			auto shiftAmount = ((opcode & (0x1F << 7)) >> 7);
			if ((shiftAmount == 0) && ((opcode & (3 << 5)) != 0))
				shiftAmount = 32;

			returnValue << "#" << shiftAmount;
		}
	}

	return returnValue.str();
}

void ARM7TDMI::unknownOpcodeArm(u32 opcode) {
	unknownOpcodeArm(opcode, "No LUT entry");
}

void ARM7TDMI::unknownOpcodeArm(u32 opcode, std::string message) {
	systemEvents.addEvent(1, bus.cpu.stopEvent, this);
	bus.log << fmt::format("Unknown opcode 0x{:0>8X} at address 0x{:0>8X}  Message: {}\n", opcode, reg.R[15] - 8, message.c_str());
}

template <bool dataTransfer, bool iBit>
bool ARM7TDMI::computeShift(u32 opcode, u32 *result) {
	u32 shiftOperand;
	u32 shiftAmount;
	bool shifterCarry = false;

	if (dataTransfer && !iBit) {
		shiftOperand = opcode & 0xFFF;
	} else if (iBit && !dataTransfer) {
		shiftOperand = opcode & 0xFF;
		shiftAmount = (opcode & (0xF << 8)) >> 7;
		if (shiftAmount == 0) {
			shifterCarry = (shiftOperand & (1 << 31)) > 0;
		} else {
			shifterCarry = shiftOperand & (1 << (shiftAmount - 1));
			shiftOperand = (shiftOperand >> shiftAmount) | (shiftOperand << (32 - shiftAmount));
		}
	} else {
		shiftOperand = reg.R[opcode & 0xF];
		shiftAmount = (opcode & (1 << 4)) ? (reg.R[(opcode >> 8) & 0xF] & 0xFF) : ((opcode & (0x1F << 7)) >> 7);
		switch ((opcode & (3 << 5)) >> 5) {
		case 0: // LSL
			if (shiftAmount != 0) {
				if (shiftAmount > 31) {
					shifterCarry = (shiftAmount == 32) ? (shiftOperand & 1) : 0;
					shiftOperand = 0;
					break;
				}
				shifterCarry = shiftOperand & (1 << (31 - (shiftAmount - 1)));
				shiftOperand <<= shiftAmount;
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
					shifterCarry = shiftOperand >> 31;
					break;
				}
			} else {
				if (shiftAmount == 0) { // RRX
					shifterCarry = shiftOperand & 1;
					shiftOperand = (shiftOperand >> 1) | (reg.flagC << 31);
					break;
				}
			}
			shifterCarry = shiftOperand & (1 << (shiftAmount - 1));
			shiftOperand = (shiftOperand >> shiftAmount) | (shiftOperand << (32 - shiftAmount));
			break;
		default:
			unknownOpcodeArm(opcode, "Shift type");
			return 0;
		}
	}

	*result = shiftOperand;
	return shifterCarry;
}
template bool ARM7TDMI::computeShift<false, false>(u32, u32*);
template bool ARM7TDMI::computeShift<false, true>(u32, u32*);
template bool ARM7TDMI::computeShift<true, false>(u32, u32*);
template bool ARM7TDMI::computeShift<true, true>(u32, u32*);

template <bool iBit, int operation, bool sBit>
void ARM7TDMI::dataProcessing(u32 opcode) {
	// Shift and rotate to get operands
	u32 operand1 = reg.R[(opcode >> 16) & 0xF];
	u32 operand2;
	bool shifterCarry = computeShift<false, iBit>(opcode, &operand2);

	// Perform operation
	bool operationCarry = reg.flagC;
	bool operationOverflow = reg.flagV;
	u32 result = 0;
	auto destinationReg = (opcode & (0xF << 12)) >> 12;
	if ((destinationReg == 15) && sBit)
		unknownOpcodeArm(opcode, "r15 as destination");
	switch (operation) {
	case 0x0: // AND
		result = operand1 & operand2;
		break;
	case 0x1: // EOR
		result = operand1 ^ operand2;
		break;
	case 0x2: // SUB
		operationCarry = operand1 >= operand2;
		result = operand1 - operand2;
		operationOverflow = ((operand1 ^ operand2) & (operand1 & result)) >> 31;
		break;
	case 0x3: // RSB
		operationCarry = operand2 >= operand1;
		result = operand2 - operand1;
		operationOverflow = ((operand2 ^ operand1) & (operand2 & result)) >> 31;
	case 0x4: // ADD
		operationOverflow = ((operand1 & 0x7FFFFFFF) + (operand2 & 0x7FFFFFFF)) >> 31;
		operationCarry = ((u64)operand1 + (u64)operand2) >> 32;
		result = operand1 + operand2;
		break;
	case 0x5: // ADC
		operationOverflow = ((operand1 & 0x7FFFFFFF) + (operand2 & 0x7FFFFFFF) + reg.flagC) >> 31;
		operationCarry = ((u64)operand1 + (u64)operand2 + reg.flagC) >> 32;
		result = operand1 + operand2 + reg.flagC;
		break;
	case 0x6: // SBC
		operationCarry = (u64)operand1 >= ((u64)operand2 + !reg.flagC);
		result = (u64)operand1 - ((u64)operand2 + !reg.flagC);
		operationOverflow = ((operand1 ^ operand2) & (operand1 & result)) >> 31;
		break;
	case 0x7: // RSC
		operationCarry = (u64)operand2 >= ((u64)operand2 + !reg.flagC);
		result = (u64)operand2 - ((u64)operand1 + !reg.flagC);
		operationOverflow = ((operand2 ^ operand1) & (operand2 & result)) >> 31;
		break;
	case 0x8: // TST
		result = operand1 & operand2;
		break;
	case 0x9: // TEQ
		result = operand1 ^ operand2;
		break;
	case 0xA: // CMP
		operationCarry = operand1 >= operand2;
		result = operand1 - operand2;
		operationOverflow = ((operand1 ^ operand2) & (operand1 & result)) >> 31;
		break;
	case 0xB: // CMN
		operationOverflow = ((operand1 & 0x7FFFFFFF) + (operand2 & 0x7FFFFFFF)) >> 31;
		operationCarry = ((u64)operand1 + (u64)operand2) >> 32;
		result = operand1 + operand2;
		break;
	case 0xC: // ORR
		result = operand1 | operand2;
		break;
	case 0xD: // MOV
		result = operand2;
		break;
	case 0xE: // BIC
		result = operand1 & (~operand2);
		break;
	case 0xF: // MVN
		result = ~operand2;
		break;
	}
	if ((operation < 8) || (operation >= 0xC)) {
		reg.R[destinationReg] = result;
		
		if (destinationReg == 15) {
			pipelineStage = 1;
			incrementR15 = false;
		}
	}

	// Compute common flags
	if (sBit) {
		reg.flagN = result >> 31; 
		reg.flagZ = result == 0;
		if ((operation < 2) || (operation == 8) || (operation == 9) || (operation >= 0xC)) { // Logical operations
			reg.flagC = shifterCarry;
		} else {
			reg.flagC = operationCarry;
			reg.flagV = operationOverflow;
		}
	}
}

template <bool prePostIndex, bool upDown, bool immediateOffset, bool writeBack, bool loadStore, int shBits>
void ARM7TDMI::halfwordDataTransfer(u32 opcode) {
	auto baseRegister = (opcode >> 16) & 0xF;
	auto srcDestRegister = (opcode >> 12) & 0xF;
	if ((baseRegister == 15) || (srcDestRegister == 15))
		unknownOpcodeArm(opcode, "r15 Operand");

	u32 offset;
	if (immediateOffset) {
		offset = ((opcode & 0xF00) >> 4) | (opcode & 0xF);
	} else {
		offset = reg.R[opcode & 0xF];
	}

	u32 address = reg.R[baseRegister];
	if (prePostIndex) {
		if (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
	}

	if (loadStore) {
		switch (shBits) {
		case 1:
			reg.R[srcDestRegister] = bus.read<u16>(address & ~1);
			break;
		default:
			unknownOpcodeArm(opcode, "SH Bits");
			return;
		}
	} else {
		switch (shBits) {
		case 1:
			bus.write<u16>(address & ~1, (u16)reg.R[srcDestRegister]);
			break;
		default:
			unknownOpcodeArm(opcode, "SH Bits");
			return;
		}
	}

	if (!prePostIndex) {
		if (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
	}
	if (writeBack || !prePostIndex) {
		reg.R[baseRegister] = address;
	}
}

template <bool immediateOffset, bool prePostIndex, bool upDown, bool byteWord, bool writeBack, bool loadStore>
void ARM7TDMI::singleDataTransfer(u32 opcode) {
	auto baseRegister = (opcode >> 16) & 0xF;
	auto srcDestRegister = (opcode >> 12) & 0xF;
	if ((baseRegister == 15) || (srcDestRegister == 15))
		unknownOpcodeArm(opcode, "r15 Operand");

	u32 offset;
	computeShift<true, immediateOffset>(opcode, &offset);

	u32 address = reg.R[baseRegister];
	if (prePostIndex) {
		if (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
	}

	if (loadStore) {
		if (byteWord) {
			reg.R[srcDestRegister] = bus.read<u8>(address);
		} else {
			reg.R[srcDestRegister] = bus.read<u32>(address & ~3);
		}
	} else {
		if (byteWord) {
			bus.write<u8>(address, reg.R[srcDestRegister]);
		} else {
			bus.write<u32>(address & ~3, reg.R[srcDestRegister]);
		}
	}

	if (!prePostIndex) {
		if (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
	}
	if (writeBack || !prePostIndex) {
		reg.R[baseRegister] = address;
	}
}

template <bool prePostIndex, bool upDown, bool sBit, bool writeBack, bool loadStore>
void ARM7TDMI::blockDataTransfer(u32 opcode) {
	if (sBit)
		unknownOpcodeArm(opcode, "LDM/STM S bit");

	u32 baseRegister = (opcode >> 16) & 0xF;
	if (baseRegister == 15)
		unknownOpcodeArm(opcode, "LDM/STM r15 as base");

	u32 address = reg.R[baseRegister] & ~3;
	u32 writeBackAddress;
	if (upDown) {
		writeBackAddress = address + std::popcount(opcode & 0xFF) * 4;

		if (prePostIndex)
			address += 4;
	} else {
		address -= std::popcount(opcode & 0xFF) * 4;
		writeBackAddress = address;

		if (!prePostIndex)
			address += 4;
	}

	if (loadStore) {
		for (int i = 0; i < 16; i++) {
			if (opcode & (1 << i)) {
				reg.R[i] = bus.read<u32>(address);
				address += 4;
			}
		}

		if (opcode & (1 << 15)) { // Treat r15 loads as jumps
			pipelineStage = 1;
			incrementR15 = false;
		}
	} else {
		for (int i = 0; i < 16; i++) {
			if (opcode & (1 << i)) {
				bus.write<u32>(address, reg.R[i]);
				address += 4;
			}
		}
	}

	if (writeBack)
		reg.R[baseRegister] = writeBackAddress;
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
	if ((lutFillIndex & armMultiplyMask) == armMultiplyBits) {
		return &ARM7TDMI::unknownOpcodeArm;
	} else if ((lutFillIndex & armMultiplyLongMask) == armMultiplyLongBits) {
		return &ARM7TDMI::unknownOpcodeArm;
	} else if ((lutFillIndex & armSingleDataSwapMask) == armSingleDataTransferBits) {
		return &ARM7TDMI::unknownOpcodeArm;
	} else if ((lutFillIndex & armBranchExchangeMask) == armBranchExchangeBits) {
		return &ARM7TDMI::unknownOpcodeArm;
	} else if ((lutFillIndex & armHalfwordDataTransferMask) == armHalfwordDataTransferBits) {
		return &ARM7TDMI::halfwordDataTransfer<(bool)(lutFillIndex & 0b0001'0000'0000), (bool)(lutFillIndex & 0b0000'1000'0000), (bool)(lutFillIndex & 0b0000'0100'0000), (bool)(lutFillIndex & 0b0000'0010'0000), (bool)(lutFillIndex & 0b0000'0001'0000), ((lutFillIndex & 0b0000'0000'0110) >> 1)>;
	} else if ((lutFillIndex & armDataProcessingMask) == armDataProcessingBits) {
		return &ARM7TDMI::dataProcessing<(bool)(lutFillIndex & 0b0010'0000'0000), ((lutFillIndex & 0b0001'1110'0000) >> 5), (bool)(lutFillIndex & 0b0000'0001'0000)>;
	} else if ((lutFillIndex & armBranchMask) == armBranchBits) {
		return &ARM7TDMI::branch<(bool)(lutFillIndex & 0b0001'0000'0000)>;
	} else if ((lutFillIndex & armSingleDataTransferMask) == armSingleDataTransferBits) {
		return &ARM7TDMI::singleDataTransfer<(bool)(lutFillIndex & 0b0010'0000'0000), (bool)(lutFillIndex & 0b0001'0000'0000), (bool)(lutFillIndex & 0b0000'1000'0000), (bool)(lutFillIndex & 0b0000'0100'0000), (bool)(lutFillIndex & 0b0000'0010'0000), (bool)(lutFillIndex & 0b0000'0001'0000)>;
	} else if ((lutFillIndex & armBlockDataTransferMask) == armBlockDataTransferBits) {
		return &ARM7TDMI::blockDataTransfer<(bool)(lutFillIndex & 0b0001'0000'0000), (bool)(lutFillIndex & 0b0000'1000'0000), (bool)(lutFillIndex & 0b0000'0100'0000), (bool)(lutFillIndex & 0b0000'0010'0000), (bool)(lutFillIndex & 0b0000'0001'0000)>;
	}

	return &ARM7TDMI::unknownOpcodeArm;
}

template <std::size_t... lutFillIndex>
constexpr std::array<lutEntry, 4096> generate_table(std::index_sequence<lutFillIndex...>) {
    return std::array{decode<lutFillIndex>()...};
}

constexpr std::array<lutEntry, 4096> ARM7TDMI::LUT = {
    generate_table(std::make_index_sequence<4096>())
};