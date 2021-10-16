
#include "arm7tdmi.hpp"
#include "gba.hpp"
#include "types.hpp"
#include <bit>
#include <cstdio>
#include <cmath>
#include <new>

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

	reg.R[0] = 0x00000000;
	reg.R[1] = 0x00000000;
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

	reg.CPSR = 0x000000DF;

	reg.R8_user = reg.R9_user = reg.R10_user = reg.R11_user = reg.R12_user = reg.R13_user = reg.R14_user = 0;
	reg.R8_fiq = reg.R9_fiq = reg.R10_fiq = reg.R11_fiq = reg.R12_fiq = reg.R13_fiq = reg.R14_fiq = reg.SPSR_fiq = 0;
	reg.R13_svc = reg.R14_svc = reg.SPSR_svc = 0;
	reg.R13_abt = reg.R14_abt = reg.SPSR_abt = 0;
	reg.R13_irq = reg.R14_irq = reg.SPSR_irq = 0;
	reg.R13_und = reg.R14_und = reg.SPSR_und = 0;

	reg.R13_irq = 0x3007FA0;
	reg.R13_svc = 0x3007FE0;
	reg.R13_fiq = reg.R13_abt = reg.R13_und = 0;//0x3007FF0;

	//incrementR15 = true;
}

void ARM7TDMI::cycle() {
	if (pipelineStage == 3) {
		if (reg.thumbMode) {
			u16 lutIndex = pipelineOpcode3 >> 6;
			(this->*thumbLUT[lutIndex])((u16)pipelineOpcode3);
		} else {
			if (checkCondition(pipelineOpcode3 >> 28)) {
				u32 lutIndex = ((pipelineOpcode3 & 0x0FF00000) >> 16) | ((pipelineOpcode3 & 0x000000F0) >> 4);
				(this->*LUT[lutIndex])(pipelineOpcode3);
			}
		}
	} else {
		++pipelineStage;
	}

	if (reg.thumbMode) {
		if (incrementR15)
			reg.R[15] += 2;
		incrementR15 = true;

		pipelineOpcode3 = pipelineOpcode2;
		pipelineOpcode2 = pipelineOpcode1;
		pipelineOpcode1 = bus.read<u16>(reg.R[15]);
	} else {
		if (incrementR15)
			reg.R[15] += 4;
		incrementR15 = true;

		pipelineOpcode3 = pipelineOpcode2;
		pipelineOpcode2 = pipelineOpcode1;
		pipelineOpcode1 = bus.read<u32>(reg.R[15]);
	}

	//if (reg.R[15] == 0x0000008)
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
static const u32 armPsrLoadMask = 0b1111'1011'1111;
static const u32 armPsrLoadBits = 0b0001'0000'0000;
static const u32 armPsrStoreRegMask = 0b1111'1011'1111;
static const u32 armPsrStoreRegBits = 0b0001'0010'0000;
static const u32 armPsrStoreImmediateMask = 0b1111'1011'0000;
static const u32 armPsrStoreImmediateBits = 0b0011'0010'0000;
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
static const u16 thumbMoveShiftedRegMask = 0b1110'0000'00;
static const u16 thumbMoveShiftedRegBits = 0b0000'0000'00;
static const u16 thumbAddSubtractMask = 0b1111'1000'00;
static const u16 thumbAddSubtractBits = 0b0001'1000'00;
static const u16 thumbAluImmediateMask = 0b1110'0000'00;
static const u16 thumbAluImmediateBits = 0b0010'0000'00;
static const u16 thumbAluRegMask = 0b1111'1100'00;
static const u16 thumbAluRegBits = 0b0100'0000'00;
static const u16 thumbHighRegOperationMask = 0b1111'1100'00;
static const u16 thumbHighRegOperationBits = 0b0100'0100'00;
static const u16 thumbPcRelativeLoadMask = 0b1111'1000'00;
static const u16 thumbPcRelativeLoadBits = 0b0100'1000'00;
static const u16 thumbLoadStoreRegOffsetMask = 0b1111'0010'00;
static const u16 thumbLoadStoreRegOffsetBits = 0b0101'0000'00;
static const u16 thumbLoadStoreSextMask = 0b1111'0010'00;
static const u16 thumbLoadStoreSextBits = 0b0101'0010'00;
static const u16 thumbLoadStoreImmediateOffsetMask = 0b1110'0000'00;
static const u16 thumbLoadStoreImmediateOffsetBits = 0b0110'0000'00;
static const u16 thumbLoadStoreHalfwordMask = 0b1111'0000'00;
static const u16 thumbLoadStoreHalfwordBits = 0b1000'0000'00;
static const u16 thumbSpRelativeLoadStoreMask = 0b1111'0000'00;
static const u16 thumbSpRelativeLoadStoreBits = 0b1001'0000'00;
static const u16 thumbLoadAddressMask = 0b1111'0000'00;
static const u16 thumbLoadAddressBits = 0b1010'0000'00;
static const u16 thumbSpAddOffsetMask = 0b1111'1111'00;
static const u16 thumbSpAddOffsetBits = 0b1011'0000'00;
static const u16 thumbPushPopRegistersMask = 0b1111'0110'00;
static const u16 thumbPushPopRegistersBits = 0b1011'0100'00;
static const u16 thumbMultipleLoadStoreMask = 0b1111'0000'00;
static const u16 thumbMultipleLoadStoreBits = 0b1100'0000'00;
static const u16 thumbConditionalBranchMask = 0b1111'0000'00;
static const u16 thumbConditionalBranchBits = 0b1101'0000'00;
static const u16 thumbSoftwareInterruptMask = 0b1111'1111'00;
static const u16 thumbSoftwareInterruptBits = 0b1101'1111'00;
static const u16 thumbUncondtionalBranchMask = 0b1111'1000'00;
static const u16 thumbUncondtionalBranchBits = 0b1110'0000'00;
static const u16 thumbLongBranchLinkMask = 0b1111'0000'00;
static const u16 thumbLongBranchLinkBits = 0b1111'0000'00;

std::string ARM7TDMI::disassemble(u32 address, u32 opcode, bool thumb) {
	std::stringstream disassembledOpcode;

	// Get condition code
	std::string condtionCode;
	switch (thumb ? ((opcode >> 8) & 0xF) : (opcode >> 28)) {
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

	if (thumb) {
		u16 lutIndex = opcode >> 6;
		if ((lutIndex & thumbAddSubtractMask) == thumbAddSubtractBits) {
			bool immediate = lutIndex & 0b0000'0100'00;
			bool op = lutIndex & 0b0000'0010'00;
			int offset = lutIndex & 0b0000'0001'11;

			disassembledOpcode << (op ? "SUB " : "ADD ");

			disassembledOpcode << getRegName(opcode & 7) << ", " << getRegName((opcode >> 3) & 7) << ", ";
			if (immediate) {
				disassembledOpcode << "#";
				if (disassemblerOptions.printOperandsHex)
					disassembledOpcode << "0x" << std::hex;
				disassembledOpcode << offset;
			} else {
				disassembledOpcode << getRegName(offset);
			}

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbMoveShiftedRegMask) == thumbMoveShiftedRegBits) {
			int op = (lutIndex & 0b0001'1000'00) >> 5;
			int shiftAmount = lutIndex & 0b0000'0111'11;

			switch (op) {
			case 0: disassembledOpcode << "LSL"; break;
			case 1: disassembledOpcode << "LSR"; break;
			case 2: disassembledOpcode << "ASR"; break;
			}

			disassembledOpcode << " " << getRegName(opcode & 7) << ", " << getRegName((opcode >> 3) & 7) << ", #";
			
			if ((shiftAmount == 0) && (op != 0))
				shiftAmount = 32;
			disassembledOpcode << shiftAmount;

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbAluImmediateMask) == thumbAluImmediateBits) {
			int op = (lutIndex & 0b0001'1000'00) >> 5;
			int destinationReg = (lutIndex & 0b0000'0111'00) >> 2;

			switch (op) {
			case 0: disassembledOpcode << "MOV"; break;
			case 1: disassembledOpcode << "CMP"; break;
			case 2: disassembledOpcode << "ADD"; break;
			case 3: disassembledOpcode << "SUB"; break;
			}

			disassembledOpcode << " " << getRegName(destinationReg) << ", #";
			if (disassemblerOptions.printOperandsHex) {
				disassembledOpcode << "0x" << std::hex;
			}
			disassembledOpcode << (opcode & 0xFF);

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbAluRegMask) == thumbAluRegBits) {
			int op = lutIndex & 0b0000'0011'11;

			switch (op) {
			case 0x0: disassembledOpcode << "AND"; break;
			case 0x1: disassembledOpcode << "EOR"; break;
			case 0x2: disassembledOpcode << "LSL"; break;
			case 0x3: disassembledOpcode << "LSR"; break;
			case 0x4: disassembledOpcode << "ASR"; break;
			case 0x5: disassembledOpcode << "ADC"; break;
			case 0x6: disassembledOpcode << "SBC"; break;
			case 0x7: disassembledOpcode << "ROR"; break;
			case 0x8: disassembledOpcode << "TST"; break;
			case 0x9: disassembledOpcode << "NEG"; break;
			case 0xA: disassembledOpcode << "CMP"; break;
			case 0xB: disassembledOpcode << "CMN"; break;
			case 0xC: disassembledOpcode << "ORR"; break;
			case 0xD: disassembledOpcode << "MUL"; break;
			case 0xE: disassembledOpcode << "BIC"; break;
			case 0xF: disassembledOpcode << "MVN"; break;
			}

			disassembledOpcode << " " << getRegName(opcode & 7) << ", " << getRegName((opcode >> 3) & 7);

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbHighRegOperationMask) == thumbHighRegOperationBits) {
			int op = (lutIndex & 0b0000'0011'00) >> 2;
			bool opFlag1 = lutIndex & 0b0000'0000'10;
			bool opFlag2 = lutIndex & 0b0000'0000'01;

			switch (op) {
			case 0: disassembledOpcode << "ADD "; break;
			case 1: disassembledOpcode << "CMP "; break;
			case 2: disassembledOpcode << "MOV "; break;
			case 3: disassembledOpcode << "BX "; break;
			}

			if (op != 3)
				disassembledOpcode << getRegName((opcode & 0x7) + (opFlag1 ? 8 : 0)) << ", ";
			disassembledOpcode << getRegName(((opcode >> 3) & 0x7) + (opFlag2 ? 8 : 0));

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbPcRelativeLoadMask) == thumbPcRelativeLoadBits) {
			int destinationReg = (lutIndex & 0b0000'0111'00) >> 2;

			disassembledOpcode << "LDR " << getRegName(destinationReg) << ", [" << getRegName(15) << ", ";
			
			if (disassemblerOptions.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << ((opcode & 0xFF) << 2) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbLoadStoreRegOffsetMask) == thumbLoadStoreRegOffsetBits) {
			bool loadStore = lutIndex & 0b0000'1000'00;
			bool byteWord = lutIndex & 0b0000'0100'00;
			int offsetReg = lutIndex & 0b0000'0001'11;

			// I was lazy and put this all on one line
			disassembledOpcode << (loadStore ? "LDR" : "STR") << (byteWord ? "B " : " ") << getRegName(opcode & 7) << ", [" << getRegName((opcode >> 3) & 7) << ", " << getRegName(offsetReg) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbLoadStoreSextMask) == thumbLoadStoreSextBits) {
			int hsBits = (lutIndex & 0b0000'1100'00) >> 4;
			int offsetReg = lutIndex & 0b0000'0001'11;

			switch (hsBits) {
			case 0: disassembledOpcode << "STRH "; break;
			case 1: disassembledOpcode << "LDSB "; break;
			case 2: disassembledOpcode << "LDRH "; break;
			case 3: disassembledOpcode << "LDSH "; break;
			}

			disassembledOpcode << getRegName(opcode & 7) << ", [" << getRegName((opcode >> 3) & 7) << ", " << getRegName(offsetReg) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbLoadStoreImmediateOffsetMask) == thumbLoadStoreImmediateOffsetBits) {
			bool byteWord = lutIndex & 0b0001'0000'00;
			bool loadStore = lutIndex & 0b0000'1000'00;
			int offset = lutIndex & 0b0000'0111'11;

			disassembledOpcode << (loadStore ? "LDR" : "STR") << (byteWord ? "B " : " ");
			disassembledOpcode << getRegName(opcode & 7) << ", [" << getRegName((opcode >> 3) & 7) << ", #";
			if (disassemblerOptions.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << (byteWord ? offset : (offset << 2)) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbLoadStoreHalfwordMask) == thumbLoadStoreHalfwordBits) {
			bool loadStore = lutIndex & 0b0000'1000'00;
			int offset = lutIndex & 0b0000'0111'11;

			disassembledOpcode << (loadStore ? "LDRH " : "STRH ");
			disassembledOpcode << getRegName(opcode & 7) << ", [" << getRegName((opcode >> 3) & 7) << ", #";
			if (disassemblerOptions.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << (offset << 1) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbSpRelativeLoadStoreMask) == thumbSpRelativeLoadStoreBits) {
			bool loadStore = lutIndex & 0b0000'1000'00;
			int destinationReg = (lutIndex & 0b0000'0111'00) >> 2;

			disassembledOpcode << (loadStore ? "LDR " : "STR ") << getRegName(destinationReg) << ", [" << getRegName(13) << ", #";
			if (disassemblerOptions.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << ((opcode & 0xFF) << 2) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbLoadAddressMask) == thumbLoadAddressBits) {
			bool spPc = lutIndex & 0b0000'1000'00;
			int destinationReg = (lutIndex & 0b0000'0111'00) >> 2;

			disassembledOpcode << "ADD " << getRegName(destinationReg) << ", " << getRegName(spPc ? 13 : 15) << ", #";
			if (disassemblerOptions.printOperandsHex) {
				disassembledOpcode << "0x" << std::hex;
			}
			disassembledOpcode << ((opcode & 0xFF) << 2);

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbSpAddOffsetMask) == thumbSpAddOffsetBits) {
			bool isNegative = lutIndex & 0b0000'0000'10;

			disassembledOpcode << "ADD sp, #";
			if (disassemblerOptions.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << (isNegative ? "-" : "") << ((opcode & 0x7F) << 2);

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbPushPopRegistersMask) == thumbPushPopRegistersBits) {
			bool loadStore = lutIndex & 0b0000'1000'00;
			bool pcLr = lutIndex & 0b0000'0001'00;

			disassembledOpcode << (loadStore ? "POP" : "PUSH") << " {";

			bool registerList[10] = {false};
			for (int i = 0; i < 8; i++)
				registerList[i] = (opcode & (1 << i));

			bool hasPrintedRegister = false;
			for (int i = 0; i < 8; i++) {
				if (registerList[i]) {
					disassembledOpcode << (hasPrintedRegister ? "," : "") << getRegName(i);
					hasPrintedRegister = true;

					if (registerList[i + 1] && registerList[i + 2]) {
						disassembledOpcode << "-";
						do {
							++i;
						} while (registerList[i + 1]);
						disassembledOpcode << getRegName(i);
					}
				}
			}
			if (pcLr)
				disassembledOpcode << (hasPrintedRegister ? "," : "") << getRegName(loadStore ? 15 : 14);
			disassembledOpcode << "}";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbMultipleLoadStoreMask) == thumbMultipleLoadStoreBits) {
			bool loadStore = lutIndex & 0b0000'1000'00;
			int baseReg = (lutIndex & 0b0000'0111'00) >> 2;

			disassembledOpcode << (loadStore ? "LDMIA " : "STMIA ") << getRegName(baseReg) << "!, {";

			bool registerList[10] = {false};
			for (int i = 0; i < 8; i++)
				registerList[i] = (opcode & (1 << i));

			bool hasPrintedRegister = false;
			for (int i = 0; i < 8; i++) {
				if (registerList[i]) {
					disassembledOpcode << (hasPrintedRegister ? "," : "") << getRegName(i);
					hasPrintedRegister = true;

					if (registerList[i + 1] && registerList[i + 2]) {
						disassembledOpcode << "-";
						do {
							++i;
						} while (registerList[i + 1]);
						disassembledOpcode << getRegName(i);
					}
				}
			}
			disassembledOpcode << "}";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbSoftwareInterruptMask) == thumbSoftwareInterruptBits) {
			if (disassemblerOptions.printAddressesHex) {
				disassembledOpcode << "SWI" << condtionCode << " #0x" << std::hex << (opcode & 0x00FF);
			} else {
				disassembledOpcode << "SWI" << condtionCode << " #" << (opcode & 0x00FF);
			}

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbConditionalBranchMask) == thumbConditionalBranchBits) {
			u32 jmpAddress = address + ((i16)((u16)opcode << 8) >> 7) + 4;

			disassembledOpcode << "B" << condtionCode << " #";
			if (disassemblerOptions.printAddressesHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << jmpAddress;

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbUncondtionalBranchMask) == thumbUncondtionalBranchBits) {
			u32 jmpAddress = address + ((i16)((u16)opcode << 5) >> 4) + 4;

			disassembledOpcode << "B #";
			if (disassemblerOptions.printAddressesHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << jmpAddress;

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbLongBranchLinkMask) == thumbLongBranchLinkBits) {
			bool lowHigh = lutIndex & 0b0000'1000'00;

			if (lowHigh) {
				disassembledOpcode << "ADD " << getRegName(14) << ", " << getRegName(14) << ", #";
				if (disassemblerOptions.printOperandsHex)
					disassembledOpcode << "0x" << std::hex;
				disassembledOpcode << ((opcode & 0x7FF) << 1);

				disassembledOpcode << "; BL " << getRegName(14);
			} else {
				disassembledOpcode << "ADD " << getRegName(14) << ", " << getRegName(15) << ", #";
				if (disassemblerOptions.printOperandsHex)
					disassembledOpcode << "0x" << std::hex;
				disassembledOpcode << ((i32)((u32)opcode << 21) >> 9);
				
			}

			return disassembledOpcode.str();
		}

		return "Undefined THUMB";
	} else {
		u32 lutIndex = ((opcode & 0x0FF00000) >> 16) | ((opcode & 0x000000F0) >> 4);
		if ((lutIndex & armMultiplyMask) == armMultiplyBits) {
			bool accumulate = lutIndex & 0b0000'0010'0000;
			bool sBit = lutIndex & 0b0000'0001'0000;

			if (accumulate) {
				disassembledOpcode << "MLA";
			} else {
				disassembledOpcode << "MUL";
			}
			disassembledOpcode << condtionCode << (sBit ? "S" : "") << " ";
			disassembledOpcode << getRegName((opcode >> 16) & 0xF) << ", " << getRegName(opcode & 0xF) << ", " << getRegName((opcode >> 8) & 0xF);

			if (accumulate)
				disassembledOpcode << ", " << getRegName((opcode >> 12) & 0xF);

			return disassembledOpcode.str();
		} else if ((lutIndex & armMultiplyLongMask) == armMultiplyLongBits) {
			bool signedMul = lutIndex & 0b0000'0100'0000;
			bool accumulate = lutIndex & 0b0000'0010'0000;
			bool sBit = lutIndex & 0b0000'0001'0000;

			disassembledOpcode << (signedMul ? "S" : "U");
			if (accumulate) {
				disassembledOpcode << "MLAL";
			} else {
				disassembledOpcode << "MULL";
			}
			disassembledOpcode << condtionCode << (sBit ? "S" : "") << " ";
			disassembledOpcode << getRegName((opcode >> 12) & 0xF) << ", " << getRegName((opcode >> 16) & 0xF) << ", " << getRegName(opcode & 0xF) << ", " << getRegName((opcode >> 8) & 0xF);

			if (accumulate)
				disassembledOpcode << ", " << getRegName((opcode >> 12) & 0xF);

			return disassembledOpcode.str();
		} else if ((lutIndex & armSingleDataSwapMask) == armSingleDataSwapBits) {
			bool byteWord = lutIndex & 0b0000'0100'0000;

			disassembledOpcode << "SWP" << condtionCode << (byteWord ? "B " : " ");
			disassembledOpcode << getRegName((opcode >> 12) & 0xF) << ", " << getRegName(opcode & 0xF) << ", [" << getRegName((opcode >> 16) & 0xF) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & armPsrLoadMask) == armPsrLoadBits) {
			bool targetPSR = lutIndex & 0b0000'0100'0000;

			disassembledOpcode << "MRS" << condtionCode << " ";
			disassembledOpcode << getRegName((opcode >> 12) & 0xF) << ", " << (targetPSR ? "SPSR" : "CPSR");

			return disassembledOpcode.str();
		} else if ((lutIndex & armPsrStoreRegMask) == armPsrStoreRegBits) {
			bool targetPSR = lutIndex & 0b0000'0100'0000;

			disassembledOpcode << "MSR" << condtionCode << " ";
			disassembledOpcode << (targetPSR ? "SPSR_" : "CPSR_");

			disassembledOpcode << ((opcode & (1 << 19)) ? "f" : "")
							<< ((opcode & (1 << 18)) ? "s" : "")
							<< ((opcode & (1 << 17)) ? "x" : "")
							<< ((opcode & (1 << 16)) ? "c" : "") << ", ";

			disassembledOpcode << getRegName(opcode & 0xF);

			return disassembledOpcode.str();
		} else if ((lutIndex & armPsrStoreImmediateMask) == armPsrStoreImmediateBits) {
			bool targetPSR = lutIndex & 0b0000'0100'0000;

			disassembledOpcode << "MSR" << condtionCode << " ";
			disassembledOpcode << (targetPSR ? "SPSR_" : "CPSR_");

			disassembledOpcode << ((opcode & (1 << 19)) ? "f" : "")
							<< ((opcode & (1 << 18)) ? "s" : "")
							<< ((opcode & (1 << 17)) ? "x" : "")
							<< ((opcode & (1 << 16)) ? "c" : "") << ", #";

			if (disassemblerOptions.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			u32 operand = opcode & 0xFF;
			u32 shiftAmount = (opcode & (0xF << 8)) >> 7;
			disassembledOpcode << (shiftAmount ? ((operand >> shiftAmount) | (operand << (32 - shiftAmount))) : operand);

			return disassembledOpcode.str();
		} else if ((lutIndex & armBranchExchangeMask) == armBranchExchangeBits) {
			disassembledOpcode << "BX" << condtionCode << " " << getRegName(opcode & 0xF);

			return disassembledOpcode.str();
		} else if ((lutIndex & armHalfwordDataTransferMask) == armHalfwordDataTransferBits) {
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
		} else if ((lutIndex & armUndefinedMask) == armUndefinedBits) {
			return "Undefined";
		} else if ((lutIndex & armSingleDataTransferMask) == armSingleDataTransferBits) {
			bool immediateOffset = lutIndex & 0b0010'0000'0000;
			bool prePostIndex = lutIndex & 0b0001'0000'0000;
			bool byteWord = lutIndex & 0b0000'0100'0000;
			bool writeBack = lutIndex & 0b0000'0010'0000;
			bool loadStore = lutIndex & 0b0000'0001'0000;

			disassembledOpcode << (loadStore ? "LDR" : "STR") << condtionCode << (byteWord ? "B " : " ");

			disassembledOpcode << getRegName((opcode >> 12) & 0xF) << ", [" << getRegName((opcode >> 16) & 0xF);

			if (immediateOffset && ((opcode & 0xFFF) == 0)) {
				disassembledOpcode << "]";
				return disassembledOpcode.str();
			}

			if (!prePostIndex) {
				disassembledOpcode << "], ";
			} else {
				disassembledOpcode << ", ";
			}
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
		} else if ((lutIndex & armSoftwareInterruptMask) == armSoftwareInterruptBits) {
			if (disassemblerOptions.printAddressesHex) {
				disassembledOpcode << "SWI" << condtionCode << " #0x" << std::hex << (opcode & 0x00FFFFFF);
			} else {
				disassembledOpcode << "SWI" << condtionCode << " #" << (opcode & 0x00FFFFFF);
			}

			return disassembledOpcode.str();
		}

		return "Undefined ARM";
	}
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
	} else if (((opcode >> 25) & 1) && !showUpDown) {
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
			returnValue << getRegName(opcode & 0xF);
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
				returnValue << ", RRX";
				return returnValue.str();
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
	bus.log << fmt::format("Unknown ARM opcode 0x{:0>8X} at address 0x{:0>7X}  Message: {}\n", opcode, reg.R[15] - 8, message.c_str());
}

void ARM7TDMI::unknownOpcodeThumb(u16 opcode) {
	unknownOpcodeThumb(opcode, "No LUT entry");
}

void ARM7TDMI::unknownOpcodeThumb(u16 opcode, std::string message) {
	systemEvents.addEvent(1, bus.cpu.stopEvent, this);
	bus.log << fmt::format("Unknown THUMB opcode 0x{:0>4X} at address 0x{:0>7X}  Message: {}\n", opcode, reg.R[15] - 4, message.c_str());
}

template <bool dataTransfer, bool iBit>
bool ARM7TDMI::computeShift(u32 opcode, u32 *result) {
	u32 shiftOperand;
	u32 shiftAmount;
	bool shifterCarry = false;
	tmpIncrement = false;

	if (dataTransfer && !iBit) {
		shiftOperand = opcode & 0xFFF;
	} else if (iBit && !dataTransfer) {
		shiftOperand = opcode & 0xFF;
		shiftAmount = (opcode & (0xF << 8)) >> 7;
		if (shiftAmount == 0) {
			shifterCarry = reg.flagC;
		} else {
			shifterCarry = shiftOperand & (1 << (shiftAmount - 1));
			shiftOperand = (shiftOperand >> shiftAmount) | (shiftOperand << (32 - shiftAmount));
		}
	} else {
		if (opcode & (1 << 4)) {
			shiftAmount = reg.R[(opcode >> 8) & 0xF] & 0xFF;

			reg.R[15] += 4;
			tmpIncrement = true;
		} else {
			shiftAmount = (opcode >> 7) & 0x1F;
		}
		shiftOperand = reg.R[opcode & 0xF];
		
		if ((opcode & (1 << 4)) && (shiftAmount == 0)) {
			shifterCarry = reg.flagC;
		} else {
			switch ((opcode >> 5) & 3) {
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
			case 1: // LSR
				if (shiftAmount == 0) {
					shifterCarry = shiftOperand >> 31;
					shiftOperand = 0;
				} else if (shiftAmount > 31) {
					shiftOperand = 0;
					shifterCarry = false;
				} else {
					shifterCarry = (shiftOperand >> (shiftAmount - 1)) & 1;
					shiftOperand = shiftOperand >> shiftAmount;
				}
				break;
			case 2: // ASR
				if ((shiftAmount == 0) || (shiftAmount > 31)) {
					if (shiftOperand & (1 << 31)) {
						shiftOperand = 0xFFFFFFFF;
						shifterCarry = true;
					} else {
						shiftOperand = 0;
						shifterCarry = false;
					}
				} else {
					shifterCarry = (shiftOperand >> (shiftAmount - 1)) & 1;
					shiftOperand = ((i32)shiftOperand) >> shiftAmount;
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
			}
		}
	}

	*result = shiftOperand;
	return shifterCarry;
}
template bool ARM7TDMI::computeShift<false, false>(u32, u32*);
template bool ARM7TDMI::computeShift<false, true>(u32, u32*);
template bool ARM7TDMI::computeShift<true, false>(u32, u32*);
template bool ARM7TDMI::computeShift<true, true>(u32, u32*);

void ARM7TDMI::bankRegisters(cpuMode newMode, bool enterMode) {
	if (reg.mode != MODE_FIQ) {
		reg.R8_user = reg.R[8];
		reg.R9_user = reg.R[9];
		reg.R10_user = reg.R[10];
		reg.R11_user = reg.R[11];
		reg.R12_user = reg.R[12];
	}

	switch (reg.mode) {
	case MODE_SYSTEM:
	case MODE_USER:
		reg.R13_user = reg.R[13];
		reg.R14_user = reg.R[14];
		break;
	case MODE_FIQ:
		reg.R8_fiq = reg.R[8];
		reg.R9_fiq = reg.R[9];
		reg.R10_fiq = reg.R[10];
		reg.R11_fiq = reg.R[11];
		reg.R12_fiq = reg.R[12];
		reg.R13_fiq = reg.R[13];
		reg.R14_fiq = reg.R[14];
		break;
	case MODE_IRQ:
		reg.R13_irq = reg.R[13];
		reg.R14_irq = reg.R[14];
		break;
	case MODE_SUPERVISOR:
		reg.R13_svc = reg.R[13];
		reg.R14_svc = reg.R[14];
		break;
	case MODE_ABORT:
		reg.R13_abt = reg.R[13];
		reg.R14_abt = reg.R[14];
		break;
	case MODE_UNDEFINED:
		reg.R13_und = reg.R[13];
		reg.R14_und = reg.R[14];
		break;
	}

	switch (newMode) {
	case MODE_SYSTEM:
	case MODE_USER:
		reg.R[8] = reg.R8_user;
		reg.R[9] = reg.R9_user;
		reg.R[10] = reg.R10_user;
		reg.R[11] = reg.R11_user;
		reg.R[12] = reg.R12_user;
		reg.R[13] = reg.R13_user;
		reg.R[14] = reg.R14_user;
		break;
	case MODE_FIQ:
		reg.R[8] = reg.R8_fiq;
		reg.R[9] = reg.R9_fiq;
		reg.R[10] = reg.R10_fiq;
		reg.R[11] = reg.R11_fiq;
		reg.R[12] = reg.R12_fiq;
		reg.R[13] = reg.R13_fiq;
		reg.R[14] = reg.R14_fiq;
		if (enterMode)
			reg.SPSR_fiq = reg.CPSR;
		break;
	case MODE_IRQ:
		reg.R[13] = reg.R13_irq;
		reg.R[14] = reg.R14_irq;
		if (enterMode)
			reg.SPSR_irq = reg.CPSR;
		break;
	case MODE_SUPERVISOR:
		reg.R[13] = reg.R13_svc;
		reg.R[14] = reg.R14_svc;
		if (enterMode)
			reg.SPSR_svc = reg.CPSR;
		break;
	case MODE_ABORT:
		reg.R[13] = reg.R13_abt;
		reg.R[14] = reg.R14_abt;
		if (enterMode)
			reg.SPSR_abt = reg.CPSR;
		break;
	case MODE_UNDEFINED:
		reg.R[13] = reg.R13_und;
		reg.R[14] = reg.R14_und;
		if (enterMode)
			reg.SPSR_und = reg.CPSR;
		break;
	default:
		printf("Unknown mode 0x%02X\n", newMode);
		return;
	}

	if (enterMode) {
		reg.CPSR = (reg.CPSR & ~0x3F) | newMode;
		reg.R[14] = reg.R[15] - (reg.thumbMode ? 2 : 4);
	}
}

void ARM7TDMI::leaveMode() {
	u32 tmpPSR = reg.CPSR;
	switch (reg.mode) {
	case MODE_FIQ: tmpPSR = reg.SPSR_fiq; break;
	case MODE_IRQ: tmpPSR = reg.SPSR_irq; break;
	case MODE_SUPERVISOR: tmpPSR = reg.SPSR_svc; break;
	case MODE_ABORT: tmpPSR = reg.SPSR_abt; break;
	case MODE_UNDEFINED: tmpPSR = reg.SPSR_und; break;
	}
	bankRegisters((cpuMode)(tmpPSR & 0x1F), false);
	reg.CPSR = tmpPSR;
}

template <bool iBit, int operation, bool sBit>
void ARM7TDMI::dataProcessing(u32 opcode) {
	// Shift and rotate to get operands
	u32 operand1;
	u32 operand2;
	bool shifterCarry = computeShift<false, iBit>(opcode, &operand2);

	// Perform operation
	bool operationCarry = reg.flagC;
	bool operationOverflow = reg.flagV;
	operand1 = reg.R[(opcode >> 16) & 0xF];
	u32 result = 0;
	auto destinationReg = (opcode & (0xF << 12)) >> 12;
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
		operationOverflow = ((operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	case 0x3: // RSB
		operationCarry = operand2 >= operand1;
		result = operand2 - operand1;
		operationOverflow = ((operand2 ^ operand1) & ((operand2 ^ result)) & 0x80000000) > 0;
		break;
	case 0x4: // ADD
		operationCarry = ((u64)operand1 + (u64)operand2) >> 32;
		result = operand1 + operand2;
		operationOverflow = (~(operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	case 0x5: // ADC
		operationCarry = ((u64)operand1 + (u64)operand2 + reg.flagC) >> 32;
		result = operand1 + operand2 + reg.flagC;
		operationOverflow = (~(operand1 ^ (operand2 + reg.flagC)) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	case 0x6: // SBC
		operationCarry = (u64)operand1 >= ((u64)operand2 + !reg.flagC);
		result = (u64)operand1 - ((u64)operand2 + !reg.flagC);
		operationOverflow = ((operand1 ^ (operand2 + !reg.flagC)) & (operand1 ^ result)) >> 31;
		break;
	case 0x7: // RSC
		operationCarry = (u64)operand2 >= ((u64)operand1 + !reg.flagC);
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
		operationOverflow = ((operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	case 0xB: // CMN
		operationCarry = ((u64)operand1 + (u64)operand2) >> 32;
		result = operand1 + operand2;
		operationOverflow = (~(operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
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

	if (tmpIncrement)
		reg.R[15] -= 4;

	if ((operation < 8) || (operation >= 0xC)) {
		reg.R[destinationReg] = result;
		
		if (destinationReg == 15) {
			pipelineStage = 1;
			incrementR15 = false;
		}
	}
	if ((destinationReg == 15) && sBit)
		leaveMode();
}

template <bool accumulate, bool sBit>
void ARM7TDMI::multiply(u32 opcode) {
	u32 destinationReg = (opcode >> 16) & 0xF;	

	u32 result = reg.R[(opcode >> 8) & 0xF] * reg.R[opcode & 0xF];
	if (accumulate)
		result += reg.R[(opcode >> 12) & 0xF];
	
	if (sBit) {
		reg.flagN = result >> 31; 
		reg.flagZ = result == 0;
	}

	reg.R[destinationReg] = result;

	if (destinationReg == 15) {
		pipelineStage = 1;
		incrementR15 = false;

		if (sBit) {
			leaveMode();
		}
	}
}

template <bool signedMul, bool accumulate, bool sBit>
void ARM7TDMI::multiplyLong(u32 opcode) {
	u32 destinationRegLow = (opcode >> 12) & 0xF;	
	u32 destinationRegHigh = (opcode >> 16) & 0xF;

	u64 result;
	if (signedMul) {
		result = (i64)((i32)reg.R[(opcode >> 8) & 0xF]) * (i64)((i32)reg.R[opcode & 0xF]);
	} else {
		result = (u64)reg.R[(opcode >> 8) & 0xF] * (u64)reg.R[opcode & 0xF];
	}
	if (accumulate)
		result += ((u64)reg.R[destinationRegHigh] << 32) | (u64)reg.R[destinationRegLow];
	
	if (sBit) {
		reg.flagN = result >> 63; 
		reg.flagZ = result == 0;
	}

	reg.R[destinationRegLow] = result;
	reg.R[destinationRegHigh] = result >> 32;

	if ((destinationRegLow == 15) || (destinationRegHigh == 15)) {
		pipelineStage = 1;
		incrementR15 = false;

		if (sBit) {
			leaveMode();
		}
	}
}

template <bool byteWord>
void ARM7TDMI::singleDataSwap(u32 opcode) {
	u32 address = reg.R[(opcode >> 16) & 0xF];
	u32 sourceRegister = opcode & 0xF;
	u32 destinationRegister = (opcode >> 12) & 0xF;

	if (byteWord) {
		reg.R[destinationRegister] = bus.read<u8>(address);
		bus.write<u8>(address, (u8)reg.R[sourceRegister]);
	} else {
		u32 result = bus.read<u32>(address & ~3);
		bus.write<u32>(address & ~3, reg.R[sourceRegister]);

		if (address & 3)
			result = (result << ((4 - (address & 3)) * 8)) | (result >> ((address & 3) * 8));

		reg.R[destinationRegister] = result;
	}

	if (destinationRegister == 15) {
		pipelineStage = 1;
		incrementR15 = false;
	}
}

template <bool targetPSR> void ARM7TDMI::psrLoad(u32 opcode) {
	u32 destinationReg = (opcode >> 12) & 0xF;

	if (targetPSR) {
		switch (reg.mode) {
		case MODE_FIQ: reg.R[destinationReg] = reg.SPSR_fiq; break;
		case MODE_IRQ: reg.R[destinationReg] = reg.SPSR_irq; break;
		case MODE_SUPERVISOR: reg.R[destinationReg] = reg.SPSR_svc; break;
		case MODE_ABORT: reg.R[destinationReg] = reg.SPSR_abt; break;
		case MODE_UNDEFINED: reg.R[destinationReg] = reg.SPSR_und; break;
		default: reg.R[destinationReg] = reg.CPSR; break;
		}
	} else {
		reg.R[destinationReg] = reg.CPSR;
	}
}

template <bool targetPSR> void ARM7TDMI::psrStoreReg(u32 opcode) {
	u32 operand = reg.R[opcode & 0xF];

	u32 *target;
	if (targetPSR) {
		switch (reg.mode) {
		case MODE_FIQ: target = &reg.SPSR_fiq; break;
		case MODE_IRQ: target = &reg.SPSR_irq; break;
		case MODE_SUPERVISOR: target = &reg.SPSR_svc; break;
		case MODE_ABORT: target = &reg.SPSR_abt; break;
		case MODE_UNDEFINED: target = &reg.SPSR_und; break;
		default: return;
		}
	} else {
		target = &reg.CPSR;
	}

	u32 result = 0;
	if (opcode & (1 << 19)) {
		result |= operand & 0xF0000000;
	} else {
		result |= *target & 0xF0000000;
	}
	if ((opcode & (1 << 16)) && reg.mode != MODE_USER) {
		result |= operand & 0x000000FF;
		if (!targetPSR)
			bankRegisters((cpuMode)(operand & 0x1F), false);
	} else {
		result |= *target & 0x000000FF;
	}

	*target = result;
}

template <bool targetPSR> void ARM7TDMI::psrStoreImmediate(u32 opcode) {
	u32 operand = opcode & 0xFF;
	u32 shiftAmount = (opcode & (0xF << 8)) >> 7;
	operand = shiftAmount ? ((operand >> shiftAmount) | (operand << (32 - shiftAmount))) : operand;

	u32 *target;
	if (targetPSR) {
		switch (reg.mode) {
		case MODE_FIQ: target = &reg.SPSR_fiq; break;
		case MODE_IRQ: target = &reg.SPSR_irq; break;
		case MODE_SUPERVISOR: target = &reg.SPSR_svc; break;
		case MODE_ABORT: target = &reg.SPSR_abt; break;
		case MODE_UNDEFINED: target = &reg.SPSR_und; break;
		default: return;
		}
	} else {
		target = &reg.CPSR;
	}

	u32 result = 0;
	if (opcode & (1 << 19)) {
		result |= operand & 0xF0000000;
	} else {
		result |= *target & 0xF0000000;
	}
	if ((opcode & (1 << 16)) && reg.mode != MODE_USER) {
		result |= operand & 0x000000FF;
		if (!targetPSR)
			bankRegisters((cpuMode)(operand & 0x1F), false);
	} else {
		result |= *target & 0x000000FF;
	}

	*target = result;
}

void ARM7TDMI::branchExchange(u32 opcode) {
	reg.thumbMode = reg.R[opcode & 0xF] & 1;

	reg.R[15] = reg.R[opcode & 0xF] & ~3;
	pipelineStage = 1;
	incrementR15 = false;
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

	u32 result = 0;
	if (loadStore) {
		switch (shBits) {
		case 1: // Unsigned Halfword
			result = bus.read<u16>(address & ~1);

			if (address & 1)
				result = (result >> 8) | (result << 24);
			break;
		case 2: // Signed Byte
			result = ((i32)((u32)bus.read<u8>(address & ~1) << 24) >> 24);
			break;
		case 3: // Signed Halfword
			result = ((i32)((u32)bus.read<u16>(address & ~1) << 16) >> 16);

			if (address & 1)
				result = (i32)result >> 8;
			break;
		default:
			unknownOpcodeArm(opcode, "SH Bits");
			return;
		}
	} else {
		switch (shBits) {
		case 1: // Unsigned Halfword
			bus.write<u16>(address & ~1, (u16)reg.R[srcDestRegister]);
			break;
		case 2: // Signed Byte
			bus.write<u8>(address, (u8)reg.R[srcDestRegister]);
			break;
		case 3: // Signed Halfword
			bus.write<u16>(address & ~1, (u16)reg.R[srcDestRegister]);
			break;
		default:
			unknownOpcodeArm(opcode, "SH Bits");
			return;
		}
	}

	if (writeBack && prePostIndex)
		reg.R[baseRegister] = address;
	if (!prePostIndex) {
		if (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
		reg.R[baseRegister] = address;
	}
	if (loadStore) {
		reg.R[srcDestRegister] = result;
		if (srcDestRegister == 15) {
			pipelineStage = 1;
			incrementR15 = false;
		}
	}
}

template <bool immediateOffset, bool prePostIndex, bool upDown, bool byteWord, bool writeBack, bool loadStore>
void ARM7TDMI::singleDataTransfer(u32 opcode) {
	auto baseRegister = (opcode >> 16) & 0xF;
	auto srcDestRegister = (opcode >> 12) & 0xF;
	if (((baseRegister == 15) && writeBack))
		unknownOpcodeArm(opcode, "r15 Operand With Writeback");

	u32 offset;
	computeShift<true, immediateOffset>(opcode, &offset);

	u32 address = reg.R[baseRegister];
	if (tmpIncrement)
		reg.R[15] -= 4;
	if (prePostIndex) {
		if (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
	}

	u32 result = 0;
	if (loadStore) { // LDR
		if (byteWord) {
			result = bus.read<u8>(address);
		} else {
			result = bus.read<u32>(address & ~3);

			// Rotate misaligned loads
			if (address & 3)
				result = (result << ((4 - (address & 3)) * 8)) | (result >> ((address & 3) * 8));
		}
	} else { // STR
		if (byteWord) {
			bus.write<u8>(address, reg.R[srcDestRegister] + (srcDestRegister == 15 ? 4 : 0));
		} else {
			bus.write<u32>(address & ~3, reg.R[srcDestRegister] + (srcDestRegister == 15 ? 4 : 0));
		}
	}

	if (writeBack && prePostIndex)
		reg.R[baseRegister] = address;
	if (!prePostIndex) {
		if (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
		reg.R[baseRegister] = address;
	}
	if (loadStore) {
		reg.R[srcDestRegister] = result;
		if (srcDestRegister == 15) {
			pipelineStage = 1;
			incrementR15 = false;
		}
	}
}

void ARM7TDMI::undefined(u32 opcode) {
	bankRegisters(MODE_UNDEFINED, true);

	reg.R[15] = 0x4;
	pipelineStage = 1;
	incrementR15 = false;
}

template <bool prePostIndex, bool upDown, bool sBit, bool writeBack, bool loadStore>
void ARM7TDMI::blockDataTransfer(u32 opcode) {
	u32 baseRegister = (opcode >> 16) & 0xF;
	if (baseRegister == 15)
		unknownOpcodeArm(opcode, "LDM/STM r15 as base");

	u32 address = reg.R[baseRegister];
	u32 writeBackAddress;
	bool emptyRegList = (opcode & 0xFFFF) == 0;
	if (upDown) {
		writeBackAddress = address + std::popcount(opcode & 0xFFFF) * 4;
		if (emptyRegList)
			writeBackAddress += 0x40;

		if (prePostIndex)
			address += 4;
	} else {
		address -= std::popcount(opcode & 0xFFFF) * 4;
		if (emptyRegList)
			address -= 0x40;
		writeBackAddress = address;

		if (!prePostIndex)
			address += 4;
	}
	address &= ~3;

	cpuMode oldMode = (cpuMode)reg.mode;
	if (sBit) {
		bankRegisters(MODE_USER, false);
		reg.mode = MODE_USER;
	}

	if (loadStore) {
		if (writeBack)
			reg.R[baseRegister] = writeBackAddress;

		if (emptyRegList) {
			reg.R[15] = bus.read<u32>(address);
			pipelineStage = 1;
			incrementR15 = false;
		} else {
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
		}
	} else {
		if (emptyRegList) {
			bus.write<u32>(address, reg.R[15] + 4);
			reg.R[baseRegister] = writeBackAddress;
		} else {
			bool firstReadWrite = true;

			for (int i = 0; i < 16; i++) {
				if (opcode & (1 << i)) {
					bus.write<u32>(address, reg.R[i] + ((((i == 15) && !firstReadWrite)) ? 4 : 0));
					address += 4;

					if (firstReadWrite) {
						if (writeBack)
							reg.R[baseRegister] = writeBackAddress;
						firstReadWrite = false;
					}
				}
			}
		}
	}

	if (sBit) {
		bankRegisters(oldMode, false);
		reg.mode = oldMode;

		if (((opcode & (1 << 15)) || emptyRegList) && loadStore)
			leaveMode();
	}
}

template <bool lBit>
void ARM7TDMI::branch(u32 opcode) {
	if (lBit)
		reg.R[14] = reg.R[15] - 4;
	reg.R[15] += ((i32)((opcode & 0x00FFFFFF) << 8)) >> 6;

	pipelineStage = 1;
	incrementR15 = false;
}

void ARM7TDMI::softwareInterrupt(u32 opcode) {
	bankRegisters(MODE_SUPERVISOR, true);

	reg.R[15] = 0x8;
	pipelineStage = 1;
	incrementR15 = false;
}

using lutEntry = void (ARM7TDMI::*)(u32);
using thumbLutEntry = void (ARM7TDMI::*)(u16);

template <std::size_t lutFillIndex>
constexpr lutEntry decode() {
	if ((lutFillIndex & armMultiplyMask) == armMultiplyBits) {
		return &ARM7TDMI::multiply<(bool)(lutFillIndex & 0b0000'0010'0000), (bool)(lutFillIndex & 0b0000'0001'0000)>;
	} else if ((lutFillIndex & armMultiplyLongMask) == armMultiplyLongBits) {
		return &ARM7TDMI::multiplyLong<(bool)(lutFillIndex & 0b0000'0100'0000), (bool)(lutFillIndex & 0b0000'0010'0000), (bool)(lutFillIndex & 0b0000'0001'0000)>;
	} else if ((lutFillIndex & armPsrLoadMask) == armPsrLoadBits) {
		return &ARM7TDMI::psrLoad<(bool)(lutFillIndex & 0b0000'0100'0000)>;
	} else if ((lutFillIndex & armPsrStoreRegMask) == armPsrStoreRegBits) {
		return &ARM7TDMI::psrStoreReg<(bool)(lutFillIndex & 0b0000'0100'0000)>;
	} else if ((lutFillIndex & armPsrStoreImmediateMask) == armPsrStoreImmediateBits) {
		return &ARM7TDMI::psrStoreImmediate<(bool)(lutFillIndex & 0b0000'0100'0000)>;
	} else if ((lutFillIndex & armSingleDataSwapMask) == armSingleDataSwapBits) {
		return &ARM7TDMI::singleDataSwap<(bool)(lutFillIndex & 0b0000'0100'0000)>;
	} else if ((lutFillIndex & armBranchExchangeMask) == armBranchExchangeBits) {
		return &ARM7TDMI::branchExchange;
	} else if ((lutFillIndex & armHalfwordDataTransferMask) == armHalfwordDataTransferBits) {
		return &ARM7TDMI::halfwordDataTransfer<(bool)(lutFillIndex & 0b0001'0000'0000), (bool)(lutFillIndex & 0b0000'1000'0000), (bool)(lutFillIndex & 0b0000'0100'0000), (bool)(lutFillIndex & 0b0000'0010'0000), (bool)(lutFillIndex & 0b0000'0001'0000), ((lutFillIndex & 0b0000'0000'0110) >> 1)>;
	} else if ((lutFillIndex & armDataProcessingMask) == armDataProcessingBits) {
		return &ARM7TDMI::dataProcessing<(bool)(lutFillIndex & 0b0010'0000'0000), ((lutFillIndex & 0b0001'1110'0000) >> 5), (bool)(lutFillIndex & 0b0000'0001'0000)>;
	} else if ((lutFillIndex & armUndefinedMask) == armUndefinedBits) {
		return &ARM7TDMI::undefined;
	} else if ((lutFillIndex & armSingleDataTransferMask) == armSingleDataTransferBits) {
		return &ARM7TDMI::singleDataTransfer<(bool)(lutFillIndex & 0b0010'0000'0000), (bool)(lutFillIndex & 0b0001'0000'0000), (bool)(lutFillIndex & 0b0000'1000'0000), (bool)(lutFillIndex & 0b0000'0100'0000), (bool)(lutFillIndex & 0b0000'0010'0000), (bool)(lutFillIndex & 0b0000'0001'0000)>;
	} else if ((lutFillIndex & armBlockDataTransferMask) == armBlockDataTransferBits) {
		return &ARM7TDMI::blockDataTransfer<(bool)(lutFillIndex & 0b0001'0000'0000), (bool)(lutFillIndex & 0b0000'1000'0000), (bool)(lutFillIndex & 0b0000'0100'0000), (bool)(lutFillIndex & 0b0000'0010'0000), (bool)(lutFillIndex & 0b0000'0001'0000)>;
	} else if ((lutFillIndex & armBranchMask) == armBranchBits) {
		return &ARM7TDMI::branch<(bool)(lutFillIndex & 0b0001'0000'0000)>;
	} else if ((lutFillIndex & armSoftwareInterruptMask) == armSoftwareInterruptBits) {
		return &ARM7TDMI::softwareInterrupt;
	}

	return &ARM7TDMI::unknownOpcodeArm;
}

template <std::size_t... lutFillIndex>
constexpr std::array<lutEntry, 4096> generateTable(std::index_sequence<lutFillIndex...>) {
    return std::array{decode<lutFillIndex>()...};
}

constexpr std::array<lutEntry, 4096> ARM7TDMI::LUT = {
    generateTable(std::make_index_sequence<4096>())
};

template <int op, int shiftAmount>
void ARM7TDMI::thumbMoveShiftedReg(u16 opcode) {
	u32 shiftOperand = reg.R[(opcode >> 3) & 7];

	switch (op) {
	case 0: // LSL
		if (shiftAmount != 0) {
			if (shiftAmount > 31) {
				reg.flagC = (shiftAmount == 32) ? (shiftOperand & 1) : 0;
				shiftOperand = 0;
				break;
			}
			reg.flagC = (bool)(shiftOperand & (1 << (31 - (shiftAmount - 1))));
			shiftOperand <<= shiftAmount;
		}
		break;
	case 1: // LSR
		if (shiftAmount == 0) {
			reg.flagC = shiftOperand >> 31;
			shiftOperand = 0;
		} else {
			reg.flagC = (shiftOperand >> (shiftAmount - 1)) & 1;
			shiftOperand = shiftOperand >> shiftAmount;
		}
		break;
	case 2: // ASR
		if (shiftAmount == 0) {
			if (shiftOperand & (1 << 31)) {
				shiftOperand = 0xFFFFFFFF;
				reg.flagC = true;
			} else {
				shiftOperand = 0;
				reg.flagC = false;
			}
		} else {
			reg.flagC = (shiftOperand >> (shiftAmount - 1)) & 1;
			shiftOperand = ((i32)shiftOperand) >> shiftAmount;
		}
		break;
	}

	reg.flagN = shiftOperand >> 31;
	reg.flagZ = shiftOperand == 0;
	reg.R[opcode & 7] = shiftOperand;
}

template <bool immediate, bool op, int offset>
void ARM7TDMI::thumbAddSubtract(u16 opcode) {
	u32 operand1 = reg.R[(opcode >> 3) & 7];
	u32 operand2 = immediate ? offset : reg.R[offset];

	u32 result;
	if (op) { // SUB
		reg.flagC = operand1 >= operand2;
		result = operand1 - operand2;
		reg.flagV = ((operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		reg.flagN = result >> 31; 
		reg.flagZ = result == 0;
	} else { // ADD
		reg.flagC = ((u64)operand1 + (u64)operand2) >> 32;
		result = operand1 + operand2;
		reg.flagV = (~(operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		reg.flagN = result >> 31; 
		reg.flagZ = result == 0;
	}

	reg.R[opcode & 7] = result;
}

template <int op, int destinationReg>
void ARM7TDMI::thumbAluImmediate(u16 opcode) {
	u32 operand1 = reg.R[destinationReg];
	u32 operand2 = opcode & 0xFF;

	u32 result;
	switch (op) {
	case 0: // MOV
		result = operand2;
		break;
	case 1: // CMP
		reg.flagC = operand1 >= operand2;
		result = operand1 - operand2;
		reg.flagV = ((operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	case 2: // ADD
		reg.flagC = ((u64)operand1 + (u64)operand2) >> 32;
		result = operand1 + operand2;
		reg.flagV = (~(operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	case 3: // SUB
		reg.flagC = operand1 >= operand2;
		result = operand1 - operand2;
		reg.flagV = ((operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	}

	reg.flagN = result >> 31;
	reg.flagZ = result == 0;
	if (op != 1)
		reg.R[destinationReg] = result;
}

template <int op>
void ARM7TDMI::thumbAluReg(u16 opcode) {
	u32 destinationReg = opcode & 7;
	u32 operand1 = reg.R[destinationReg];
	u32 operand2 = reg.R[(opcode >> 3) & 7];

	u32 result;
	switch (op) {
	case 0x0: // AND
		result = operand1 & operand2;
		break;
	case 0x1: // EOR
		result = operand1 ^ operand2;
		break;
	case 0x2: // LSL
		if (operand2 == 0) {
			result = operand1;
		} else {
			if (operand2 > 31) {
				reg.flagC = (operand2 == 32) ? (operand1 & 1) : 0;
				result = 0;
				break;
			}
			reg.flagC = (operand1 & (1 << (31 - (operand2 - 1)))) >> 31;
			result = operand1 << operand2;
		}
		break;
	case 0x3: // LSR
		if (operand2 == 0) {
			result = operand1;
		} else if (operand2 == 32) {
			result = 0;
			reg.flagC = operand1 >> 31;
		} else if (operand2 > 32) {
			result = 0;
			reg.flagC = false;
		} else {
			reg.flagC = (operand1 >> (operand2 - 1)) & 1;
			result = operand1 >> operand2;
		}
		break;
	case 0x4: // ASR
		if (operand2 == 0) {
			result = operand1;
		} else if (operand2 > 31) {
			if (operand1 & (1 << 31)) {
				result = 0xFFFFFFFF;
				reg.flagC = true;
			} else {
				result = 0;
				reg.flagC = false;
			}
		} else {
			reg.flagC = (operand1 >> (operand2 - 1)) & 1;
			result = ((i32)operand1) >> operand2;
		}
		break;
	case 0x5: // ADC
		result = operand1 + operand2 + reg.flagC;
		reg.flagC = ((u64)operand1 + (u64)operand2 + reg.flagC) >> 32;
		reg.flagV = (~(operand1 ^ (operand2 + reg.flagC)) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	case 0x6: // SBC
		result = (u64)operand1 - ((u64)operand2 + !reg.flagC);
		reg.flagC = (u64)operand1 >= ((u64)operand2 + !reg.flagC);
		reg.flagV = ((operand1 ^ (operand2 + !reg.flagC)) & (operand1 ^ result)) >> 31;
		break;
	case 0x7: // ROR
		if (operand2 == 0) {
			result = operand1;
		} else {
			operand2 &= 31;
			if (operand2 == 0) {
				reg.flagC = operand1 >> 31;
				result = operand1;
				break;
			}
			reg.flagC = (bool)(operand1 & (1 << (operand2 - 1)));
			result = (operand1 >> operand2) | (operand1 << (32 - operand2));
		}
		break;
	case 0x8: // TST
		result = operand1 & operand2;
		break;
	case 0x9: // NEG
		reg.flagC = 0 >= operand2;
		result = 0 - operand2;
		reg.flagV = (operand2 & result & 0x80000000) > 0;
		break;
	case 0xA: // CMP
		reg.flagC = operand1 >= operand2;
		result = operand1 - operand2;
		reg.flagV = ((operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	case 0xB: // CMN
		reg.flagC = ((u64)operand1 + (u64)operand2) >> 32;
		result = operand1 + operand2;
		reg.flagV = (~(operand1 ^ operand2) & ((operand1 ^ result)) & 0x80000000) > 0;
		break;
	case 0xC: // ORR
		result = operand1 | operand2;
		break;
	case 0xD: // MUL
		result = operand1 * operand2;
		reg.flagC = 0;
		break;
	case 0xE: // BIC
		result = operand1 & (~operand2);
		break;
	case 0xF: // MVN
		result = ~operand2;
		break;
	}

	// Compute common flags
	reg.flagN = result >> 31; 
	reg.flagZ = result == 0;

	if ((op != 0x8) && (op != 0xA) && (op != 0xB))
		reg.R[destinationReg] = result;
}

template <int op, bool opFlag1, bool opFlag2>
void ARM7TDMI::thumbHighRegOperation(u16 opcode) {
	u32 operand1 = (opcode & 0x7) + (opFlag1 ? 8 : 0);
	u32 operand2 = ((opcode >> 3) & 0x7) + (opFlag2 ? 8 : 0);

	u32 result;
	switch (op) {
	case 0: // ADD
		reg.R[operand1] += reg.R[operand2];
		break;
	case 1: // CMP
		reg.flagC = reg.R[operand1] >= reg.R[operand2];
		result = reg.R[operand1] - reg.R[operand2];
		reg.flagV = ((reg.R[operand1] ^ reg.R[operand2]) & ((reg.R[operand1] ^ result)) & 0x80000000) > 0;
		reg.flagN = result >> 31; 
		reg.flagZ = result == 0;
		break;
	case 2: // MOV
		reg.R[operand1] = reg.R[operand2];
		break;
	case 3: // BX
		reg.thumbMode = reg.R[operand2] & 1;

		reg.R[15] = reg.R[operand2] & ~1;
		pipelineStage = 1;
		incrementR15 = false;
		break;
	}

	if (operand1 == 15) {
		reg.R[15] &= ~1;
		pipelineStage = 1;
		incrementR15 = false;
	}
}

template <int destinationReg>
void ARM7TDMI::thumbPcRelativeLoad(u16 opcode) {
	reg.R[destinationReg] = bus.read<u32>((reg.R[15] + ((opcode & 0xFF) << 2)) & ~3);
}

template <bool loadStore, bool byteWord, int offsetReg>
void ARM7TDMI::thumbLoadStoreRegOffset(u16 opcode) {
	auto srcDestRegister = opcode & 0x7;
	u32 address = reg.R[(opcode >> 3) & 7] + reg.R[offsetReg];

	if (loadStore) {
		if (byteWord) { // LDRB
			reg.R[srcDestRegister] = bus.read<u8>(address);
		} else { // LDR
			u32 result = bus.read<u32>(address & ~3);

			if (address & 3)
				result = (result << ((4 - (address & 3)) * 8)) | (result >> ((address & 3) * 8));
			
			reg.R[srcDestRegister] = result;
		}
	} else {
		if (byteWord) {
			bus.write<u8>(address, (u8)reg.R[srcDestRegister]);
		} else {
			bus.write<u32>(address & ~3, reg.R[srcDestRegister]);
		}
	}
}

template <int hsBits, int offsetReg>
void ARM7TDMI::thumbLoadStoreSext(u16 opcode) {
	auto srcDestRegister = opcode & 0x7;
	u32 address = reg.R[(opcode >> 3) & 7] + reg.R[offsetReg];

	u32 result = 0;
	switch (hsBits) {
	case 0: // STRH
		bus.write<u16>(address & ~1, (u16)reg.R[srcDestRegister]);
		break;
	case 1: // LDSB
		result = bus.read<u8>(address);
		result = (i32)(result << 24) >> 24;
		break;
	case 2: // LDRH
		result = bus.read<u16>(address & ~1);

		if (address & 1)
			result = (result >> 8) | (result << 24);
		break;
	case 3: // LDSH
		result = bus.read<u16>(address & ~1);
		result = (i32)(result << 16) >> 16;

		if (address & 1)
			result = (i32)result >> 8;
		break;
	}

	if (hsBits != 0)
		reg.R[srcDestRegister] = result;
}

template <bool byteWord, bool loadStore, int offset>
void ARM7TDMI::thumbLoadStoreImmediateOffset(u16 opcode) {
	auto srcDestRegister = opcode & 0x7;
	u32 address = reg.R[(opcode >> 3) & 7] + (byteWord ? offset : (offset << 2));

	if (loadStore) {
		if (byteWord) { // LDRB
			reg.R[srcDestRegister] = bus.read<u8>(address);
		} else { // LDR
			u32 result = bus.read<u32>(address & ~3);

			if (address & 3)
				result = (result << ((4 - (address & 3)) * 8)) | (result >> ((address & 3) * 8));
			
			reg.R[srcDestRegister] = result;
		}
	} else {
		if (byteWord) {
			bus.write<u8>(address, (u8)reg.R[srcDestRegister]);
		} else {
			bus.write<u32>(address & ~3, reg.R[srcDestRegister]);
		}
	}
}

template <bool loadStore, int offset>
void ARM7TDMI::thumbLoadStoreHalfword(u16 opcode) {
	auto srcDestRegister = opcode & 0x7;
	u32 address = reg.R[(opcode >> 3) & 7] + (offset << 1);

	if (loadStore) { // LDRH
		u32 result = bus.read<u16>(address & ~1);

		if (address & 1)
			result = (result >> 8) | (result << 24);

		reg.R[srcDestRegister] = result;
	} else { // STRH
		bus.write<u16>(address & ~1, (u16)reg.R[srcDestRegister]);
	}
}

template <bool loadStore, int destinationReg>
void ARM7TDMI::thumbSpRelativeLoadStore(u16 opcode) {
	u32 address = reg.R[13] + ((opcode & 0xFF) << 2);

	if (loadStore) {
		u32 result = bus.read<u32>(address & ~3);

		if (address & 3)
			result = (result << ((4 - (address & 3)) * 8)) | (result >> ((address & 3) * 8));
		
		reg.R[destinationReg] = result;
	} else {
		bus.write<u32>(address & ~3, reg.R[destinationReg]);
	}
}

template <bool spPc, int destinationReg>
void ARM7TDMI::thumbLoadAddress(u16 opcode) {
	if (spPc) {
		reg.R[destinationReg] = reg.R[13] + ((opcode & 0xFF) << 2);
	} else {
		reg.R[destinationReg] = (reg.R[15] & ~3) + ((opcode & 0xFF) << 2);
	}
}

template <bool isNegative>
void ARM7TDMI::thumbSpAddOffset(u16 opcode) {
	u32 operand = (opcode & 0x7F) << 2;

	if (isNegative) {
		reg.R[13] -= operand;
	} else {
		reg.R[13] += operand;
	}
}

template <bool loadStore, bool pcLr>
void ARM7TDMI::thumbPushPopRegisters(u16 opcode) {
	u32 address = reg.R[13];
	u32 writeBackAddress;
	bool emptyRegList = ((opcode & 0xFF) == 0) && !pcLr;

	if (loadStore) { // POP/LDMIA!
		writeBackAddress = address + std::popcount((u32)opcode & 0xFF) * 4;
		if (emptyRegList)
			writeBackAddress += 0x40;
		address &= ~3;

		reg.R[13] = writeBackAddress + (pcLr * 4);

		if (emptyRegList) {
			reg.R[15] = bus.read<u32>(address);
			pipelineStage = 1;
			incrementR15 = false;
		} else {
			for (int i = 0; i < 8; i++) {
				if (opcode & (1 << i)) {
					reg.R[i] = bus.read<u32>(address);
					address += 4;
				}
			}
			if (pcLr) {
				reg.R[15] = bus.read<u32>(address) & ~1;
				pipelineStage = 1;
				incrementR15 = false;
			}
		}
	} else { // PUSH/STMDB!
		address -= (std::popcount((u32)opcode & 0xFF) + pcLr) * 4;
		if (emptyRegList)
			address -= 0x40;
		writeBackAddress = address;
		address &= ~3;

		if (emptyRegList) {
			bus.write<u32>(address, reg.R[15] + 2);
			reg.R[13] = writeBackAddress;
		} else {
			for (int i = 0; i < 8; i++) {
				if (opcode & (1 << i)) {
					bus.write<u32>(address, reg.R[i]);
					address += 4;
				}
			}
			if (pcLr)
				bus.write<u32>(address, reg.R[14]);

			reg.R[13] = writeBackAddress;
		}
	}
}

template <bool loadStore, int baseReg>
void ARM7TDMI::thumbMultipleLoadStore(u16 opcode) {
	u32 address = reg.R[baseReg];
	u32 writeBackAddress;
	bool emptyRegList = (opcode & 0xFF) == 0;

	writeBackAddress = address + std::popcount((u32)opcode & 0xFF) * 4;
	if (emptyRegList)
		writeBackAddress += 0x40;
	address &= ~3;

	if (loadStore) {
		if (!(opcode & (1 << baseReg)))
			reg.R[baseReg] = writeBackAddress;

		if (emptyRegList) {
			reg.R[15] = bus.read<u32>(address);
			pipelineStage = 1;
			incrementR15 = false;
		} else {
			for (int i = 0; i < 8; i++) {
				if (opcode & (1 << i)) {
					reg.R[i] = bus.read<u32>(address);
					address += 4;
				}
			}
		}
	} else {
		if (emptyRegList) {
			bus.write<u32>(address, reg.R[15] + 2);
			reg.R[baseReg] = writeBackAddress;
		} else {
			bool firstReadWrite = true;

			for (int i = 0; i < 8; i++) {
				if (opcode & (1 << i)) {
					bus.write<u32>(address, reg.R[i]);
					address += 4;

					if (firstReadWrite) {
						reg.R[baseReg] = writeBackAddress;
						firstReadWrite = false;
					}
				}
			}
		}
	}
}

template <int condition>
void ARM7TDMI::thumbConditionalBranch(u16 opcode) {
	if (checkCondition(condition)) {
		reg.R[15] += ((i16)(opcode << 8) >> 7);
		pipelineStage = 1;
		incrementR15 = false;
	}
}

void ARM7TDMI::thumbSoftwareInterrupt(u16 opcode) {
	bankRegisters(MODE_SUPERVISOR, true);

	reg.R[15] = 0x8;
	pipelineStage = 1;
	incrementR15 = false;
}

void ARM7TDMI::thumbUncondtionalBranch(u16 opcode) {
	reg.R[15] += (i16)((u16)opcode << 5) >> 4;
	pipelineStage = 1;
	incrementR15 = false;
}

template <bool lowHigh>
void ARM7TDMI::thumbLongBranchLink(u16 opcode) {
	if (lowHigh) {
		u32 address = reg.R[14] + ((opcode & 0x7FF) << 1);
		reg.R[14] = (reg.R[15] - 2) | 1;
		reg.R[15] = address;
		pipelineStage = 1;
		incrementR15 = false;
	} else {
		reg.R[14] = reg.R[15] + ((i32)((u32)opcode << 21) >> 9);
	}
}

template <std::size_t lutFillIndex>
constexpr thumbLutEntry decodeThumb() {
	if ((lutFillIndex & thumbAddSubtractMask) == thumbAddSubtractBits) {
		return &ARM7TDMI::thumbAddSubtract<(bool)(lutFillIndex & 0b0000'0100'00), (bool)(lutFillIndex & 0b0000'0010'00), (lutFillIndex & 0b0000'0001'11)>;
	} else if ((lutFillIndex & thumbMoveShiftedRegMask) == thumbMoveShiftedRegBits) {
		return &ARM7TDMI::thumbMoveShiftedReg<((lutFillIndex & 0b0001'1000'00) >> 5), (lutFillIndex & 0b0000'0111'11)>;
	} else if ((lutFillIndex & thumbAluImmediateMask) == thumbAluImmediateBits) {
		return &ARM7TDMI::thumbAluImmediate<((lutFillIndex & 0b0001'1000'00) >> 5), ((lutFillIndex & 0b0000'0111'00) >> 2)>;
	} else if ((lutFillIndex & thumbAluRegMask) == thumbAluRegBits) {
		return &ARM7TDMI::thumbAluReg<(lutFillIndex & 0b0000'0011'11)>;
	} else if ((lutFillIndex & thumbHighRegOperationMask) == thumbHighRegOperationBits) {
		return &ARM7TDMI::thumbHighRegOperation<((lutFillIndex & 0b0000'0011'00) >> 2), (bool)(lutFillIndex & 0b0000'0000'10), (bool)(lutFillIndex & 0b0000'0000'01)>;
	} else if ((lutFillIndex & thumbPcRelativeLoadMask) == thumbPcRelativeLoadBits) {
		return &ARM7TDMI::thumbPcRelativeLoad<((lutFillIndex & 0b0000'0111'00) >> 2)>;
	} else if ((lutFillIndex & thumbLoadStoreRegOffsetMask) == thumbLoadStoreRegOffsetBits) {
		return &ARM7TDMI::thumbLoadStoreRegOffset<(bool)(lutFillIndex & 0b0000'1000'00), (bool)(lutFillIndex & 0b0000'0100'00), (lutFillIndex & 0b0000'0001'11)>;
	} else if ((lutFillIndex & thumbLoadStoreSextMask) == thumbLoadStoreSextBits) {
		return &ARM7TDMI::thumbLoadStoreSext<((lutFillIndex & 0b0000'1100'00) >> 4), (lutFillIndex & 0b0000'0001'11)>;
	} else if ((lutFillIndex & thumbLoadStoreImmediateOffsetMask) == thumbLoadStoreImmediateOffsetBits) {
		return &ARM7TDMI::thumbLoadStoreImmediateOffset<(bool)(lutFillIndex & 0b0001'0000'00), (bool)(lutFillIndex & 0b0000'1000'00), (lutFillIndex & 0b0000'0111'11)>;
	} else if ((lutFillIndex & thumbLoadStoreHalfwordMask) == thumbLoadStoreHalfwordBits) {
		return &ARM7TDMI::thumbLoadStoreHalfword<(bool)(lutFillIndex & 0b0000'1000'00), (lutFillIndex & 0b0000'0111'11)>;
	} else if ((lutFillIndex & thumbSpRelativeLoadStoreMask) == thumbSpRelativeLoadStoreBits) {
		return &ARM7TDMI::thumbSpRelativeLoadStore<(bool)(lutFillIndex & 0b0000'1000'00), ((lutFillIndex & 0b0000'0111'00) >> 2)>;
	} else if ((lutFillIndex & thumbLoadAddressMask) == thumbLoadAddressBits) {
		return &ARM7TDMI::thumbLoadAddress<(bool)(lutFillIndex & 0b0000'1000'00), ((lutFillIndex & 0b0000'0111'00) >> 2)>;
	} else if ((lutFillIndex & thumbSpAddOffsetMask) == thumbSpAddOffsetBits) {
		return &ARM7TDMI::thumbSpAddOffset<(bool)(lutFillIndex & 0b0000'0000'10)>;
	} else if ((lutFillIndex & thumbPushPopRegistersMask) == thumbPushPopRegistersBits) {
		return &ARM7TDMI::thumbPushPopRegisters<(bool)(lutFillIndex & 0b0000'1000'00), (bool)(lutFillIndex & 0b0000'0001'00)>;
	} else if ((lutFillIndex & thumbMultipleLoadStoreMask) == thumbMultipleLoadStoreBits) {
		return &ARM7TDMI::thumbMultipleLoadStore<(bool)(lutFillIndex & 0b0000'1000'00), ((lutFillIndex & 0b0000'0111'00) >> 2)>;
	} else if ((lutFillIndex & thumbSoftwareInterruptMask) == thumbSoftwareInterruptBits) {
		return &ARM7TDMI::thumbSoftwareInterrupt;
	} else if ((lutFillIndex & thumbConditionalBranchMask) == thumbConditionalBranchBits) {
		return &ARM7TDMI::thumbConditionalBranch<((lutFillIndex & 0b0000'1111'00) >> 2)>;
	} else if ((lutFillIndex & thumbUncondtionalBranchMask) == thumbUncondtionalBranchBits) {
		return &ARM7TDMI::thumbUncondtionalBranch;
	} else if ((lutFillIndex & thumbLongBranchLinkMask) == thumbLongBranchLinkBits) {
		return &ARM7TDMI::thumbLongBranchLink<(bool)(lutFillIndex & 0b0000'1000'00)>;
	}

	return &ARM7TDMI::unknownOpcodeThumb;
}

template <std::size_t... lutFillIndex>
constexpr std::array<thumbLutEntry, 1024> generateTableThumb(std::index_sequence<lutFillIndex...>) {
    return std::array{decodeThumb<lutFillIndex>()...};
}

constexpr std::array<thumbLutEntry, 1024> ARM7TDMI::thumbLUT = {
    generateTableThumb(std::make_index_sequence<1024>())
};