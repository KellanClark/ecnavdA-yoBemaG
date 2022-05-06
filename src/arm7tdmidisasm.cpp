
#include "arm7tdmidisasm.hpp"

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
static const u16 thumbUnconditionalBranchMask = 0b1111'1000'00;
static const u16 thumbUnconditionalBranchBits = 0b1110'0000'00;
static const u16 thumbLongBranchLinkMask = 0b1111'0000'00;
static const u16 thumbLongBranchLinkBits = 0b1111'0000'00;

std::string ARM7TDMIDisassembler::disassemble(u32 address, u32 opcode, bool thumb) {
	std::stringstream disassembledOpcode;

	// Get condition code
	std::string conditionCode;
	switch (thumb ? ((opcode >> 8) & 0xF) : (opcode >> 28)) {
	case 0x0: conditionCode = "EQ"; break;
	case 0x1: conditionCode = "NE"; break;
	case 0x2: conditionCode = "CS"; break;
	case 0x3: conditionCode = "CC"; break;
	case 0x4: conditionCode = "MI"; break;
	case 0x5: conditionCode = "PL"; break;
	case 0x6: conditionCode = "VS"; break;
	case 0x7: conditionCode = "VC"; break;
	case 0x8: conditionCode = "HI"; break;
	case 0x9: conditionCode = "LS"; break;
	case 0xA: conditionCode = "GE"; break;
	case 0xB: conditionCode = "LT"; break;
	case 0xC: conditionCode = "GT"; break;
	case 0xD: conditionCode = "LE"; break;
	case 0xE: conditionCode = options.showALCondition ? "AL" : ""; break;
	default:
		if (thumb) {
			conditionCode = "Undefined";
			break;
		} else {
			return "Undefined";
		}
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
				if (options.printOperandsHex)
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
			if (options.printOperandsHex) {
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

			if (options.printOperandsHex)
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
			if (options.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << (byteWord ? offset : (offset << 2)) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbLoadStoreHalfwordMask) == thumbLoadStoreHalfwordBits) {
			bool loadStore = lutIndex & 0b0000'1000'00;
			int offset = lutIndex & 0b0000'0111'11;

			disassembledOpcode << (loadStore ? "LDRH " : "STRH ");
			disassembledOpcode << getRegName(opcode & 7) << ", [" << getRegName((opcode >> 3) & 7) << ", #";
			if (options.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << (offset << 1) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbSpRelativeLoadStoreMask) == thumbSpRelativeLoadStoreBits) {
			bool loadStore = lutIndex & 0b0000'1000'00;
			int destinationReg = (lutIndex & 0b0000'0111'00) >> 2;

			disassembledOpcode << (loadStore ? "LDR " : "STR ") << getRegName(destinationReg) << ", [" << getRegName(13) << ", #";
			if (options.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << ((opcode & 0xFF) << 2) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbLoadAddressMask) == thumbLoadAddressBits) {
			bool spPc = lutIndex & 0b0000'1000'00;
			int destinationReg = (lutIndex & 0b0000'0111'00) >> 2;

			disassembledOpcode << "ADD " << getRegName(destinationReg) << ", " << getRegName(spPc ? 13 : 15) << ", #";
			if (options.printOperandsHex) {
				disassembledOpcode << "0x" << std::hex;
			}
			disassembledOpcode << ((opcode & 0xFF) << 2);

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbSpAddOffsetMask) == thumbSpAddOffsetBits) {
			bool isNegative = lutIndex & 0b0000'0000'10;

			disassembledOpcode << "ADD sp, #";
			if (options.printOperandsHex)
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
			if (options.printOperandsHex) {
				disassembledOpcode << "SWI" << " #0x" << std::hex << (opcode & 0x00FF);
			} else {
				disassembledOpcode << "SWI" << " #" << (opcode & 0x00FF);
			}

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbConditionalBranchMask) == thumbConditionalBranchBits) {
			u32 jmpAddress = address + ((i16)((u16)opcode << 8) >> 7) + 4;

			disassembledOpcode << "B" << conditionCode << " #";
			if (options.printAddressesHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << jmpAddress;

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbUnconditionalBranchMask) == thumbUnconditionalBranchBits) {
			u32 jmpAddress = address + ((i16)((u16)opcode << 5) >> 4) + 4;

			disassembledOpcode << "B #";
			if (options.printAddressesHex)
				disassembledOpcode << "0x" << std::hex;
			disassembledOpcode << jmpAddress;

			return disassembledOpcode.str();
		} else if ((lutIndex & thumbLongBranchLinkMask) == thumbLongBranchLinkBits) {
			bool lowHigh = lutIndex & 0b0000'1000'00;

			if (lowHigh) {
				disassembledOpcode << "ADD " << getRegName(14) << ", " << getRegName(14) << ", #";
				if (options.printOperandsHex)
					disassembledOpcode << "0x" << std::hex;
				disassembledOpcode << ((opcode & 0x7FF) << 1);

				disassembledOpcode << "; BL " << getRegName(14);
			} else {
				disassembledOpcode << "ADD " << getRegName(14) << ", " << getRegName(15) << ", #";
				if (options.printOperandsHex)
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
			disassembledOpcode << conditionCode << (sBit ? "S" : "") << " ";
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
			disassembledOpcode << conditionCode << (sBit ? "S" : "") << " ";
			disassembledOpcode << getRegName((opcode >> 12) & 0xF) << ", " << getRegName((opcode >> 16) & 0xF) << ", " << getRegName(opcode & 0xF) << ", " << getRegName((opcode >> 8) & 0xF);

			if (accumulate)
				disassembledOpcode << ", " << getRegName((opcode >> 12) & 0xF);

			return disassembledOpcode.str();
		} else if ((lutIndex & armSingleDataSwapMask) == armSingleDataSwapBits) {
			bool byteWord = lutIndex & 0b0000'0100'0000;

			disassembledOpcode << "SWP" << conditionCode << (byteWord ? "B " : " ");
			disassembledOpcode << getRegName((opcode >> 12) & 0xF) << ", " << getRegName(opcode & 0xF) << ", [" << getRegName((opcode >> 16) & 0xF) << "]";

			return disassembledOpcode.str();
		} else if ((lutIndex & armPsrLoadMask) == armPsrLoadBits) {
			bool targetPSR = lutIndex & 0b0000'0100'0000;

			disassembledOpcode << "MRS" << conditionCode << " ";
			disassembledOpcode << getRegName((opcode >> 12) & 0xF) << ", " << (targetPSR ? "SPSR" : "CPSR");

			return disassembledOpcode.str();
		} else if ((lutIndex & armPsrStoreRegMask) == armPsrStoreRegBits) {
			bool targetPSR = lutIndex & 0b0000'0100'0000;

			disassembledOpcode << "MSR" << conditionCode << " ";
			disassembledOpcode << (targetPSR ? "SPSR_" : "CPSR_");

			disassembledOpcode << ((opcode & (1 << 19)) ? "f" : "")
							<< ((opcode & (1 << 18)) ? "s" : "")
							<< ((opcode & (1 << 17)) ? "x" : "")
							<< ((opcode & (1 << 16)) ? "c" : "") << ", ";

			disassembledOpcode << getRegName(opcode & 0xF);

			return disassembledOpcode.str();
		} else if ((lutIndex & armPsrStoreImmediateMask) == armPsrStoreImmediateBits) {
			bool targetPSR = lutIndex & 0b0000'0100'0000;

			disassembledOpcode << "MSR" << conditionCode << " ";
			disassembledOpcode << (targetPSR ? "SPSR_" : "CPSR_");

			disassembledOpcode << ((opcode & (1 << 19)) ? "f" : "")
							<< ((opcode & (1 << 18)) ? "s" : "")
							<< ((opcode & (1 << 17)) ? "x" : "")
							<< ((opcode & (1 << 16)) ? "c" : "") << ", #";

			if (options.printOperandsHex)
				disassembledOpcode << "0x" << std::hex;
			u32 operand = opcode & 0xFF;
			u32 shiftAmount = (opcode & (0xF << 8)) >> 7;
			disassembledOpcode << (shiftAmount ? ((operand >> shiftAmount) | (operand << (32 - shiftAmount))) : operand);

			return disassembledOpcode.str();
		} else if ((lutIndex & armBranchExchangeMask) == armBranchExchangeBits) {
			disassembledOpcode << "BX" << conditionCode << " " << getRegName(opcode & 0xF);

			return disassembledOpcode.str();
		} else if ((lutIndex & armHalfwordDataTransferMask) == armHalfwordDataTransferBits) {
			bool prePostIndex = lutIndex & 0b0001'0000'0000;
			bool upDown = lutIndex & 0b0000'1000'0000;
			bool immediateOffset = lutIndex & 0b0000'0100'0000;
			bool writeBack = lutIndex & 0b0000'0010'0000;
			bool loadStore = lutIndex & 0b0000'0001'0000;
			int shBits = (lutIndex & 0b0000'0000'0110) >> 1;

			disassembledOpcode << (loadStore ? "LDR" : "STR") << conditionCode;
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

			if ((printRd && sBit) || options.alwaysShowSBit)
				disassembledOpcode << "S";
			disassembledOpcode << conditionCode << " ";
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

			disassembledOpcode << (loadStore ? "LDR" : "STR") << conditionCode << (byteWord ? "B " : " ");

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

			if (options.simplifyPushPop && (baseRegister == 13) && ((loadStore && !prePostIndex && upDown) || (!loadStore && prePostIndex && !upDown)) && writeBack && !sBit) {
				disassembledOpcode << (loadStore ? "POP" : "PUSH") << conditionCode << " {";
			} else {
				disassembledOpcode << (loadStore ? "LDM" : "STM") << conditionCode;

				// Code based on Table 4-6: Addressing mode names
				if (options.ldmStmStackSuffixes) {
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
			disassembledOpcode << conditionCode;

			u32 jumpLocation = address + (((i32)((opcode & 0x00FFFFFF) << 8)) >> 6) + 8;
			if (options.printAddressesHex) {
				disassembledOpcode << " #0x" << std::hex << jumpLocation;
			} else {
				disassembledOpcode << " #" << jumpLocation;
			}

			return disassembledOpcode.str();
		} else if ((lutIndex & armSoftwareInterruptMask) == armSoftwareInterruptBits) {
			if (options.printAddressesHex) {
				disassembledOpcode << "SWI" << conditionCode << " #0x" << std::hex << (opcode & 0x00FFFFFF);
			} else {
				disassembledOpcode << "SWI" << conditionCode << " #" << (opcode & 0x00FFFFFF);
			}

			return disassembledOpcode.str();
		}

		return "Undefined ARM";
	}
}

std::string ARM7TDMIDisassembler::getRegName(unsigned int regNumber) {
	if (options.simplifyRegisterNames) {
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

std::string ARM7TDMIDisassembler::disassembleShift(u32 opcode, bool showUpDown) {
	std::stringstream returnValue;

	if (showUpDown && !((opcode >> 25) & 1)) {
		returnValue << ((showUpDown & !((opcode >> 23) & 1)) ? "#-" : "#");
		if (options.printOperandsHex) {
			returnValue << "0x" << std::hex;
		}
		returnValue << (opcode & 0xFFF);

		return returnValue.str();
	} else if (((opcode >> 25) & 1) && !showUpDown) {
		u32 shiftAmount = (opcode & (0xF << 8)) >> 7;
		u32 shiftInput = opcode & 0xFF;
		shiftInput = shiftAmount ? ((shiftInput >> shiftAmount) | (shiftInput << (32 - shiftAmount))) : shiftInput;

		if (options.printOperandsHex) {
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

void ARM7TDMIDisassembler::defaultSettings() {
    options.showALCondition = false;
	options.alwaysShowSBit = false;
	options.printOperandsHex = true;
	options.printAddressesHex = true;
	options.simplifyRegisterNames = false;
	options.simplifyPushPop = false;
	options.ldmStmStackSuffixes = false;
}

ARM7TDMIDisassembler disassembler;