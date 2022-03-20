
#include "arm7tdmi.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include "types.hpp"
#include <bit>
#include <cstdio>

#define iCycle(x) bus.internalCycle(x)

ARM7TDMI::ARM7TDMI(GameBoyAdvance& bus_) : bus(bus_) {
	resetARM7TDMI();
}

void ARM7TDMI::resetARM7TDMI() {
	processIrq = false;

	pipelineStage = 0;

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
	reg.R[15] = 0;//0x08000000; // Start of ROM

	reg.CPSR = 0x000000DF;

	reg.R8_user = reg.R9_user = reg.R10_user = reg.R11_user = reg.R12_user = reg.R13_user = reg.R14_user = 0;
	reg.R8_fiq = reg.R9_fiq = reg.R10_fiq = reg.R11_fiq = reg.R12_fiq = reg.R13_fiq = reg.R14_fiq = reg.SPSR_fiq = 0;
	reg.R13_svc = reg.R14_svc = reg.SPSR_svc = 0;
	reg.R13_abt = reg.R14_abt = reg.SPSR_abt = 0;
	reg.R13_irq = reg.R14_irq = reg.SPSR_irq = 0;
	reg.R13_und = reg.R14_und = reg.SPSR_und = 0;

	reg.R13_irq = 0x3007FA0;
	reg.R13_svc = 0x3007FE0;
	reg.R13_fiq = reg.R13_abt = reg.R13_und = 0x3007FF0;
}

void ARM7TDMI::cycle() {
	if (pipelineStage == 0)
		flushPipeline();
	if (processIrq) { // Service interrupt
		processIrq = false;

		//fetchOpcode();
		//reg.R[15] -= reg.thumbMode ? 2 : 4;
		bool oldThumb = reg.thumbMode;
		bankRegisters(MODE_IRQ, true);
		reg.R[14] = reg.R[15] - (oldThumb ? 0 : 4);

		reg.irqDisable = true;
		reg.fiqDisable = true;
		reg.thumbMode = false;

		reg.R[15] = 0x18;
		flushPipeline();
	} else {
		if (reg.thumbMode) {
			u16 lutIndex = pipelineOpcode3 >> 6;
			(this->*thumbLUT[lutIndex])((u16)pipelineOpcode3);
		} else {
			if (checkCondition(pipelineOpcode3 >> 28)) {
				u32 lutIndex = ((pipelineOpcode3 & 0x0FF00000) >> 16) | ((pipelineOpcode3 & 0x000000F0) >> 4);
				(this->*LUT[lutIndex])(pipelineOpcode3);
			} else {
				fetchOpcode();
			}
		}
	}

	//if (reg.R[15] == (0x8000180 + 4))
	//	unknownOpcodeArm(pipelineOpcode3, "BKPT");
	//if (reg.R[15] == (0x8025396 + 4))
	//	printf("0x%08X\n", reg.R[1]);
}

inline bool ARM7TDMI::checkCondition(int condtionCode) {
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
	case 0xF: return true;
	default:
		unknownOpcodeArm(pipelineOpcode3, "Invalid condition");
		return false;
	}
}

inline void ARM7TDMI::fetchOpcode() {
	if (reg.thumbMode) {
		pipelineOpcode1 = bus.read<u16>(reg.R[15], nextFetchType);
		pipelineOpcode3 = pipelineOpcode2;
		pipelineOpcode2 = pipelineOpcode1;

		reg.R[15] += 2;
	} else {
		pipelineOpcode1 = bus.read<u32>(reg.R[15], nextFetchType);
		pipelineOpcode3 = pipelineOpcode2;
		pipelineOpcode2 = pipelineOpcode1;

		reg.R[15] += 4;
	}

	nextFetchType = true;
}

void ARM7TDMI::flushPipeline() {
	pipelineStage = 3;

	if (reg.thumbMode) {
		reg.R[15] &= ~1;
		pipelineOpcode3 = bus.read<u16>(reg.R[15], false);
		pipelineOpcode2 = bus.read<u16>(reg.R[15] + 2, true);
		reg.R[15] += 4;
	} else {
		reg.R[15] &= ~3;
		pipelineOpcode3 = bus.read<u32>(reg.R[15], false);
		pipelineOpcode2 = bus.read<u32>(reg.R[15] + 4, true);
		reg.R[15] += 8;
	}

	nextFetchType = true;
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

	if constexpr (dataTransfer && !iBit) {
		shiftOperand = opcode & 0xFFF;
	} else if constexpr (iBit && !dataTransfer) {
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
				if ((shiftAmount == 0) || (shiftAmount == 32)) {
					shifterCarry = shiftOperand >> 31;
					shiftOperand = 0;
				} else if (shiftAmount > 32) {
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
		//reg.R[14] = reg.R[15] - (reg.thumbMode ? 2 : 4);
		reg.CPSR = (reg.CPSR & ~0x3F) | newMode;
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

	bool shiftReg = !iBit && ((opcode >> 4) & 1);
	if (shiftReg) {
		fetchOpcode();
	}
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
		operationOverflow = (~(operand1 ^ operand2) & ((operand1 ^ result))) >> 31;
		break;
	case 0x6: // SBC
		operationCarry = (u64)operand1 >= ((u64)operand2 + !reg.flagC);
		result = (u64)operand1 - ((u64)operand2 + !reg.flagC);
		operationOverflow = ((operand1 ^ operand2) & (operand1 ^ result)) >> 31;
		break;
	case 0x7: // RSC
		operationCarry = (u64)operand2 >= ((u64)operand1 + !reg.flagC);
		result = (u64)operand2 - ((u64)operand1 + !reg.flagC);
		operationOverflow = ((operand2 ^ operand1) & (operand2 ^ result)) >> 31;
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
	if constexpr (sBit) {
		reg.flagN = result >> 31;
		reg.flagZ = result == 0;
		if ((operation < 2) || (operation == 8) || (operation == 9) || (operation >= 0xC)) { // Logical operations
			reg.flagC = shifterCarry;
		} else {
			reg.flagC = operationCarry;
			reg.flagV = operationOverflow;
		}
	}

	if (shiftReg) {
		iCycle(1); // TODO: Should probably be after setting register
	} else {
		fetchOpcode();
	}

	if constexpr ((operation < 8) || (operation >= 0xC)) {
		reg.R[destinationReg] = result;

		if (destinationReg == 15) {
			if (sBit)
				leaveMode();

			flushPipeline();
		}
	} else if constexpr (sBit) {
		if (destinationReg == 15)
			leaveMode();
	}
}

template <bool accumulate, bool sBit>
void ARM7TDMI::multiply(u32 opcode) {
	u32 destinationReg = (opcode >> 16) & 0xF;
	u32 multiplier = reg.R[(opcode >> 8) & 0xF];
	fetchOpcode();

	u32 result = multiplier * reg.R[opcode & 0xF];
	if constexpr (accumulate) {
		result += reg.R[(opcode >> 12) & 0xF];
		iCycle(1);
	}
	reg.R[destinationReg] = result;
	if constexpr (sBit) {
		reg.flagN = result >> 31;
		reg.flagZ = result == 0;
	}

	int multiplierCycles = ((31 - std::max(std::countl_zero(multiplier), std::countl_one(multiplier))) / 8) + 1;
	iCycle(multiplierCycles);

	if (destinationReg == 15) {
		if constexpr (sBit)
			leaveMode();

		flushPipeline();
	}
}

template <bool signedMul, bool accumulate, bool sBit>
void ARM7TDMI::multiplyLong(u32 opcode) {
	u32 destinationRegLow = (opcode >> 12) & 0xF;
	u32 destinationRegHigh = (opcode >> 16) & 0xF;
	u32 multiplier = reg.R[(opcode >> 8) & 0xF];
	fetchOpcode();

	u64 result;
	int multiplierCycles;
	if constexpr (signedMul) {
		result = (i64)((i32)multiplier) * (i64)((i32)reg.R[opcode & 0xF]);
		multiplierCycles = ((31 - std::max(std::countl_zero(multiplier), std::countl_one(multiplier))) / 8) + 1;
	} else {
		result = (u64)multiplier * (u64)reg.R[opcode & 0xF];
		multiplierCycles = ((31 - std::countl_zero(multiplier)) / 8) + 1;
	}
	if constexpr (accumulate) {
		result += ((u64)reg.R[destinationRegHigh] << 32) | (u64)reg.R[destinationRegLow];
		iCycle(1);
	}
	if constexpr (sBit) {
		reg.flagN = result >> 63;
		reg.flagZ = result == 0;
	}

	iCycle(multiplierCycles + 1);

	reg.R[destinationRegLow] = result;
	reg.R[destinationRegHigh] = result >> 32;

	nextFetchType = true;
	if ((destinationRegLow == 15) || (destinationRegHigh == 15)) {
		if constexpr (sBit)
			leaveMode();

		flushPipeline();
	}
}

template <bool byteWord>
void ARM7TDMI::singleDataSwap(u32 opcode) {
	u32 address = reg.R[(opcode >> 16) & 0xF];
	u32 sourceRegister = opcode & 0xF;
	u32 destinationRegister = (opcode >> 12) & 0xF;
	u32 result;
	fetchOpcode();

	if constexpr (byteWord) {
		result = bus.read<u8>(address, true);
		bus.write<u8>(address, (u8)reg.R[sourceRegister], false);
	} else {
		result = bus.read<u32>(address, true);
		bus.write<u32>(address, reg.R[sourceRegister], false);
	}

	reg.R[destinationRegister] = result;
	iCycle(1);

	if (destinationRegister == 15) {
		flushPipeline();
	}
}

template <bool targetPSR> void ARM7TDMI::psrLoad(u32 opcode) {
	u32 destinationReg = (opcode >> 12) & 0xF;

	if constexpr (targetPSR) {
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

	fetchOpcode();
}

template <bool targetPSR> void ARM7TDMI::psrStoreReg(u32 opcode) {
	u32 operand = reg.R[opcode & 0xF];

	u32 *target;
	if constexpr (targetPSR) {
		switch (reg.mode) {
		case MODE_FIQ: target = &reg.SPSR_fiq; break;
		case MODE_IRQ: target = &reg.SPSR_irq; break;
		case MODE_SUPERVISOR: target = &reg.SPSR_svc; break;
		case MODE_ABORT: target = &reg.SPSR_abt; break;
		case MODE_UNDEFINED: target = &reg.SPSR_und; break;
		default: fetchOpcode(); return;
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
		if constexpr (!targetPSR)
			bankRegisters((cpuMode)(operand & 0x1F), false);
	} else {
		result |= *target & 0x000000FF;
	}

	*target = result;
	fetchOpcode();
}

template <bool targetPSR> void ARM7TDMI::psrStoreImmediate(u32 opcode) {
	u32 operand = opcode & 0xFF;
	u32 shiftAmount = (opcode & (0xF << 8)) >> 7;
	operand = shiftAmount ? ((operand >> shiftAmount) | (operand << (32 - shiftAmount))) : operand;

	u32 *target;
	if constexpr (targetPSR) {
		switch (reg.mode) {
		case MODE_FIQ: target = &reg.SPSR_fiq; break;
		case MODE_IRQ: target = &reg.SPSR_irq; break;
		case MODE_SUPERVISOR: target = &reg.SPSR_svc; break;
		case MODE_ABORT: target = &reg.SPSR_abt; break;
		case MODE_UNDEFINED: target = &reg.SPSR_und; break;
		default: fetchOpcode(); return;
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
		if constexpr (!targetPSR)
			bankRegisters((cpuMode)(operand & 0x1F), false);
	} else {
		result |= *target & 0x000000FF;
	}

	*target = result;
	fetchOpcode();
}

void ARM7TDMI::branchExchange(u32 opcode) {
	bool newThumb = reg.R[opcode & 0xF] & 1;
	u32 newAddress = reg.R[opcode & 0xF] & (newThumb ? ~1 : ~3);
	fetchOpcode();

	reg.thumbMode = newThumb;
	reg.R[15] = newAddress;
	flushPipeline();
}

template <bool prePostIndex, bool upDown, bool immediateOffset, bool writeBack, bool loadStore, int shBits>
void ARM7TDMI::halfwordDataTransfer(u32 opcode) {
	auto baseRegister = (opcode >> 16) & 0xF;
	auto srcDestRegister = (opcode >> 12) & 0xF;
	if ((baseRegister == 15) && writeBack)
		unknownOpcodeArm(opcode, "r15 Operand With Writeback");

	u32 offset;
	if constexpr (immediateOffset) {
		offset = ((opcode & 0xF00) >> 4) | (opcode & 0xF);
	} else {
		offset = reg.R[opcode & 0xF];
	}

	u32 address = reg.R[baseRegister];
	if constexpr (prePostIndex) {
		if constexpr (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
	}
	fetchOpcode();

	u32 result = 0;
	if constexpr (loadStore) {
		if constexpr (shBits == 1) { // LDRH
			result = bus.read<u16>(address, false);
		} else if constexpr (shBits == 2) { // LDRSB
			result = ((i32)((u32)bus.read<u8>(address, false) << 24) >> 24);
		} else if constexpr (shBits == 3) { // LDRSH
			result = bus.read<u16>(address, false);

			if (address & 1) {
				result = (i32)(result << 24) >> 24;
			} else {
				result = (i32)(result << 16) >> 16;
			}
		}
	} else {
		if constexpr (shBits == 1) { // STRH
			bus.write<u16>(address, (u16)reg.R[srcDestRegister], false);
		}

		nextFetchType = false;
	}

	// TODO: Base register modification should be at same time as read/write
	if constexpr (writeBack && prePostIndex)
		reg.R[baseRegister] = address;
	if constexpr (!prePostIndex) {
		if constexpr (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
		reg.R[baseRegister] = address;
	}
	if constexpr (loadStore) {
		reg.R[srcDestRegister] = result;
		iCycle(1);

		if (srcDestRegister == 15) {
			flushPipeline();
		}
	}
}

template <bool immediateOffset, bool prePostIndex, bool upDown, bool byteWord, bool writeBack, bool loadStore>
void ARM7TDMI::singleDataTransfer(u32 opcode) {
	auto baseRegister = (opcode >> 16) & 0xF;
	auto srcDestRegister = (opcode >> 12) & 0xF;
	if ((baseRegister == 15) && writeBack)
		unknownOpcodeArm(opcode, "r15 Operand With Writeback");

	u32 offset;
	computeShift<true, immediateOffset>(opcode, &offset);

	u32 address = reg.R[baseRegister];
	if constexpr (prePostIndex) {
		if constexpr (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
	}
	fetchOpcode();

	u32 result = 0;
	if constexpr (loadStore) { // LDR
		if constexpr (byteWord) {
			result = bus.read<u8>(address, false);
		} else {
			result = bus.read<u32>(address, false);
		}
	} else { // STR
		if constexpr (byteWord) {
			bus.write<u8>(address, reg.R[srcDestRegister], false);
		} else {
			bus.write<u32>(address, reg.R[srcDestRegister], false);
		}

		nextFetchType = false;
	}

	// TODO: Base register modification should be at same time as read/write
	if constexpr (writeBack && prePostIndex)
		reg.R[baseRegister] = address;
	if constexpr (!prePostIndex) {
		if constexpr (upDown) {
			address += offset;
		} else {
			address -= offset;
		}
		reg.R[baseRegister] = address;
	}
	if constexpr (loadStore) {
		reg.R[srcDestRegister] = result;
		iCycle(1);

		if (srcDestRegister == 15) {
			flushPipeline();
		}
	}
}

void ARM7TDMI::undefined(u32 opcode) {
	bankRegisters(MODE_UNDEFINED, true);
	reg.R[14] = reg.R[15] - 4;
	fetchOpcode();

	reg.R[15] = 0x4;
	flushPipeline();
}

template <bool prePostIndex, bool upDown, bool sBit, bool writeBack, bool loadStore>
void ARM7TDMI::blockDataTransfer(u32 opcode) {
	u32 baseRegister = (opcode >> 16) & 0xF;
	if ((baseRegister == 15) && writeBack)
		unknownOpcodeArm(opcode, "r15 Operand With Writeback");

	u32 address = reg.R[baseRegister];
	u32 writeBackAddress;
	bool emptyRegList = (opcode & 0xFFFF) == 0;
	if constexpr (upDown) {
		writeBackAddress = address + std::popcount(opcode & 0xFFFF) * 4;
		if (emptyRegList)
			writeBackAddress += 0x40;

		if constexpr (prePostIndex)
			address += 4;
	} else {
		address -= std::popcount(opcode & 0xFFFF) * 4;
		if (emptyRegList)
			address -= 0x40;
		writeBackAddress = address;

		if constexpr (!prePostIndex)
			address += 4;
	}

	cpuMode oldMode = (cpuMode)reg.mode;
	if constexpr (sBit) {
		bankRegisters(MODE_USER, false);
		reg.mode = MODE_USER;
	}
	fetchOpcode();

	bool firstReadWrite = true; // TODO: Interleave fetches with register writes
	if constexpr (loadStore) { // LDM
		if constexpr (writeBack)
			reg.R[baseRegister] = writeBackAddress;

		if (emptyRegList) { // TODO: find timings for empty list
			reg.R[baseRegister] = writeBackAddress;
			reg.R[15] = bus.read<u32>(address, false);
			flushPipeline();
		} else {
			for (int i = 0; i < 16; i++) {
				if (opcode & (1 << i)) {
					if (firstReadWrite) {
						if constexpr (writeBack)
							reg.R[baseRegister] = writeBackAddress;
					}

					u32 val = bus.read<u32>(address, !firstReadWrite);
					reg.R[i] = (val >> ((4 - (address & 3)) * 8)) | (val << ((address & 3) * 8)); // Undo rotate
					address += 4;

					if (firstReadWrite)
						firstReadWrite = false;
				}
			}
			iCycle(1);

			if (opcode & (1 << 15)) { // Treat r15 loads as jumps
				flushPipeline();
			}
		}
	} else { // STM
		if (emptyRegList) {
			bus.write<u32>(address, reg.R[15], false);
			reg.R[baseRegister] = writeBackAddress;
		} else {
			for (int i = 0; i < 16; i++) {
				if (opcode & (1 << i)) {
					bus.write<u32>(address, reg.R[i], !firstReadWrite);
					address += 4;

					if (firstReadWrite) {
						if constexpr (writeBack)
							reg.R[baseRegister] = writeBackAddress;
						firstReadWrite = false;
					}
				}
			}
		}

		nextFetchType = false;
	}

	if constexpr (sBit) {
		bankRegisters(oldMode, false);
		reg.mode = oldMode;

		if (((opcode & (1 << 15)) || emptyRegList) && loadStore) {
			leaveMode();
		}
	}
}

template <bool lBit>
void ARM7TDMI::branch(u32 opcode) {
	u32 address = reg.R[15] + (((i32)((opcode & 0x00FFFFFF) << 8)) >> 6);
	fetchOpcode();

	if constexpr (lBit)
		reg.R[14] = reg.R[15] - 8;
	reg.R[15] = address;
	flushPipeline();
}

void ARM7TDMI::softwareInterrupt(u32 opcode) { // TODO: Proper timings for exceptions
	fetchOpcode();
	bankRegisters(MODE_SUPERVISOR, true);
	reg.R[14] = reg.R[15] - 8;

	reg.R[15] = 0x8;
	flushPipeline();
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
	fetchOpcode();
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
	fetchOpcode();
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
	if constexpr (op != 1)
		reg.R[destinationReg] = result;
	fetchOpcode();
}

template <int op>
void ARM7TDMI::thumbAluReg(u16 opcode) {
	u32 destinationReg = opcode & 7;
	u32 operand1 = reg.R[destinationReg];
	u32 operand2 = reg.R[(opcode >> 3) & 7];

	constexpr bool writeResult = ((op != 0x8) && (op != 0xA) && (op != 0xB));
	constexpr bool endWithIdle = ((op == 0x2) || (op == 0x3) || (op == 0x4) || (op == 0x7) || (op == 0xD));

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
			} else {
				reg.flagC = (operand1 & (1 << (31 - (operand2 - 1)))) > 0;
				result = operand1 << operand2;
			}
		}
		fetchOpcode();
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
		fetchOpcode();
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
		fetchOpcode();
		break;
	case 0x5: // ADC
		result = operand1 + operand2 + reg.flagC;
		reg.flagC = ((u64)operand1 + (u64)operand2 + reg.flagC) >> 32;
		reg.flagV = (~(operand1 ^ operand2) & ((operand1 ^ result))) >> 31;
		break;
	case 0x6: // SBC
		result = (u64)operand1 - ((u64)operand2 + !reg.flagC);
		reg.flagC = (u64)operand1 >= ((u64)operand2 + !reg.flagC);
		reg.flagV = ((operand1 ^ operand2) & (operand1 ^ result)) >> 31;
		break;
	case 0x7: // ROR
		if (operand2 == 0) {
			result = operand1;
		} else {
			operand2 &= 31;
			if (operand2 == 0) {
				reg.flagC = operand1 >> 31;
				result = operand1;
			} else {
				reg.flagC = (bool)(operand1 & (1 << (operand2 - 1)));
				result = (operand1 >> operand2) | (operand1 << (32 - operand2));
			}
		}
		fetchOpcode();
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
		fetchOpcode();
		iCycle((31 - std::max(std::countl_zero(operand1), std::countl_one(operand1))) / 8);

		result = operand1 * operand2;
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

	if constexpr (writeResult)
		reg.R[destinationReg] = result;
	if constexpr (endWithIdle) {
		iCycle(1);
	} else {
		fetchOpcode();
	}
}

template <int op, bool opFlag1, bool opFlag2>
void ARM7TDMI::thumbHighRegOperation(u16 opcode) {
	u32 operand1 = (opcode & 0x7) + (opFlag1 ? 8 : 0);
	u32 operand2 = ((opcode >> 3) & 0x7) + (opFlag2 ? 8 : 0);

	u32 result;
	switch (op) {
	case 0: // ADD
		result = reg.R[operand1] + reg.R[operand2];
		break;
	case 1: // CMP
		reg.flagC = reg.R[operand1] >= reg.R[operand2];
		result = reg.R[operand1] - reg.R[operand2];
		reg.flagV = ((reg.R[operand1] ^ reg.R[operand2]) & ((reg.R[operand1] ^ result)) & 0x80000000) > 0;
		reg.flagN = result >> 31;
		reg.flagZ = result == 0;
		break;
	case 2: // MOV
		result = reg.R[operand2];
		break;
	case 3:{ // BX
		bool newThumb = reg.R[operand2] & 1;
		u32 newAddress = reg.R[operand2];
		fetchOpcode();

		reg.thumbMode = newThumb;
		reg.R[15] = newAddress;
		flushPipeline();
		}return;
	}
	fetchOpcode();
	if constexpr (op != 1)
		reg.R[operand1] = result;

	if (operand1 == 15) {
		flushPipeline();
	}
}

template <int destinationReg>
void ARM7TDMI::thumbPcRelativeLoad(u16 opcode) {
	u32 address = (reg.R[15] + ((opcode & 0xFF) << 2)) & ~3;
	fetchOpcode();

	reg.R[destinationReg] = bus.read<u32>(address, false);
	iCycle(1);
}

template <bool loadStore, bool byteWord, int offsetReg>
void ARM7TDMI::thumbLoadStoreRegOffset(u16 opcode) {
	auto srcDestRegister = opcode & 0x7;
	u32 address = reg.R[(opcode >> 3) & 7] + reg.R[offsetReg];
	fetchOpcode();

	u32 result;
	if constexpr (loadStore) {
		if constexpr (byteWord) { // LDRB
			result = bus.read<u8>(address, false);
		} else { // LDR
			result = bus.read<u32>(address, false);
		}
	} else {
		if constexpr (byteWord) { // STRB
			bus.write<u8>(address, (u8)reg.R[srcDestRegister], false);
		} else { // STR
			bus.write<u32>(address, reg.R[srcDestRegister], false);
		}

		nextFetchType = false;
	}

	if constexpr (loadStore) {
		reg.R[srcDestRegister] = result;
		iCycle(1);
	}
}

template <int hsBits, int offsetReg>
void ARM7TDMI::thumbLoadStoreSext(u16 opcode) {
	auto srcDestRegister = opcode & 0x7;
	u32 address = reg.R[(opcode >> 3) & 7] + reg.R[offsetReg];
	fetchOpcode();

	u32 result = 0;
	switch (hsBits) {
	case 0: // STRH
		bus.write<u16>(address, (u16)reg.R[srcDestRegister], false);
		nextFetchType = false;
		break;
	case 1: // LDSB
		result = bus.read<u8>(address, false);
		result = (i32)(result << 24) >> 24;
		break;
	case 2: // LDRH
		result = bus.read<u16>(address, false);
		break;
	case 3: // LDSH
		result = bus.read<u16>(address, false);

		if (address & 1) {
			result = (i32)(result << 24) >> 24;
		} else {
			result = (i32)(result << 16) >> 16;
		}
		break;
	}

	if constexpr (hsBits != 0) {
		reg.R[srcDestRegister] = result;
		iCycle(1);
	}
}

template <bool byteWord, bool loadStore, int offset>
void ARM7TDMI::thumbLoadStoreImmediateOffset(u16 opcode) {
	auto srcDestRegister = opcode & 0x7;
	u32 address = reg.R[(opcode >> 3) & 7] + (byteWord ? offset : (offset << 2));
	fetchOpcode();

	if constexpr (loadStore) {
		if constexpr (byteWord) { // LDRB
			reg.R[srcDestRegister] = bus.read<u8>(address, false);
		} else { // LDR
			reg.R[srcDestRegister] = bus.read<u32>(address, false);
		}
		iCycle(1);
	} else {
		if constexpr (byteWord) { // STRB
			bus.write<u8>(address, (u8)reg.R[srcDestRegister], false);
		} else { // STR
			bus.write<u32>(address, reg.R[srcDestRegister], false);
		}

		nextFetchType = false;
	}
}

template <bool loadStore, int offset>
void ARM7TDMI::thumbLoadStoreHalfword(u16 opcode) {
	auto srcDestRegister = opcode & 0x7;
	u32 address = reg.R[(opcode >> 3) & 7] + (offset << 1);
	fetchOpcode();

	if constexpr (loadStore) { // LDRH
		reg.R[srcDestRegister] = bus.read<u16>(address, false);
		iCycle(1);
	} else { // STRH
		bus.write<u16>(address, (u16)reg.R[srcDestRegister], false);

		nextFetchType = false;
	}
}

template <bool loadStore, int destinationReg>
void ARM7TDMI::thumbSpRelativeLoadStore(u16 opcode) {
	u32 address = reg.R[13] + ((opcode & 0xFF) << 2);
	fetchOpcode();

	if constexpr (loadStore) {
		reg.R[destinationReg] = bus.read<u32>(address, false);

		iCycle(1);
	} else {
		bus.write<u32>(address, reg.R[destinationReg], false);

		nextFetchType = false;
	}
}

template <bool spPc, int destinationReg>
void ARM7TDMI::thumbLoadAddress(u16 opcode) {
	if constexpr (spPc) {
		reg.R[destinationReg] = reg.R[13] + ((opcode & 0xFF) << 2);
	} else {
		reg.R[destinationReg] = (reg.R[15] & ~3) + ((opcode & 0xFF) << 2);
	}
	fetchOpcode();
}

template <bool isNegative>
void ARM7TDMI::thumbSpAddOffset(u16 opcode) {
	u32 operand = (opcode & 0x7F) << 2;

	if constexpr (isNegative) {
		reg.R[13] -= operand;
	} else {
		reg.R[13] += operand;
	}
	fetchOpcode();
}

template <bool loadStore, bool pcLr>
void ARM7TDMI::thumbPushPopRegisters(u16 opcode) {
	u32 address = reg.R[13];
	bool emptyRegList = ((opcode & 0xFF) == 0) && !pcLr;

	bool firstReadWrite = true;
	if constexpr (loadStore) { // POP/LDMIA!
		u32 writeBackAddress = address + std::popcount((u32)opcode & 0xFF) * 4;
		if (emptyRegList)
			writeBackAddress += 0x40;
		reg.R[13] = writeBackAddress + (pcLr * 4);
		fetchOpcode(); // Writeback really should be inside the main loop but this works

		if (emptyRegList) {
			reg.R[15] = bus.read<u32>(address, false);
			flushPipeline();
		} else {
			for (int i = 0; i < 8; i++) {
				if (opcode & (1 << i)) {
					u32 val = bus.read<u32>(address, !firstReadWrite);
					reg.R[i] = (val >> ((4 - (address & 3)) * 8)) | (val << ((address & 3) * 8)); // Undo rotate
					address += 4;

					if (firstReadWrite)
						firstReadWrite = false;
				}
			}
			iCycle(1);
			if constexpr (pcLr) {
				reg.R[15] = bus.read<u32>(address, true);
				flushPipeline();
			}
		}
	} else { // PUSH/STMDB!
		address -= (std::popcount((u32)opcode & 0xFF) + pcLr) * 4;
		if (emptyRegList)
			address -= 0x40;
		reg.R[13] = address;
		fetchOpcode();

		if (emptyRegList) {
			bus.write<u32>(address, reg.R[15] + 2, false);
		} else {
			for (int i = 0; i < 8; i++) {
				if (opcode & (1 << i)) {
					bus.write<u32>(address, reg.R[i], !firstReadWrite);
					address += 4;
				}
			}
			if constexpr (pcLr)
				bus.write<u32>(address, reg.R[14], true);
		}
		nextFetchType = false;
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
	fetchOpcode();

	bool firstReadWrite = true;
	if constexpr (loadStore) { // LDMIA!
		if (emptyRegList) {
			reg.R[baseReg] = writeBackAddress;
			reg.R[15] = bus.read<u32>(address, true);
			flushPipeline();
		} else {
			for (int i = 0; i < 8; i++) {
				if (opcode & (1 << i)) {
					if (firstReadWrite)
						reg.R[baseReg] = writeBackAddress;

					u32 val = bus.read<u32>(address, !firstReadWrite);
					reg.R[i] = (val >> ((4 - (address & 3)) * 8)) | (val << ((address & 3) * 8)); // Undo rotate
					address += 4;

					if (firstReadWrite)
						firstReadWrite = false;
				}
			}
			iCycle(1);
		}
	} else { // STMIA!
		if (emptyRegList) {
			bus.write<u32>(address, reg.R[15], false);
			reg.R[baseReg] = writeBackAddress;
		} else {
			for (int i = 0; i < 8; i++) {
				if (opcode & (1 << i)) {
					bus.write<u32>(address, reg.R[i], !firstReadWrite);
					address += 4;

					if (firstReadWrite) {
						reg.R[baseReg] = writeBackAddress;
						firstReadWrite = false;
					}
				}
			}
		}
		nextFetchType = false;
	}
}

template <int condition>
void ARM7TDMI::thumbConditionalBranch(u16 opcode) {
	u32 newAddress = reg.R[15] + ((i16)(opcode << 8) >> 7);
	fetchOpcode();

	if (checkCondition(condition)) {
		reg.R[15] = newAddress;
		flushPipeline();
	}
}

void ARM7TDMI::thumbSoftwareInterrupt(u16 opcode) {
	fetchOpcode();
	bankRegisters(MODE_SUPERVISOR, true);
	reg.R[14] = reg.R[15] - 4;

	reg.R[15] = 0x8;
	flushPipeline();
}

void ARM7TDMI::thumbUncondtionalBranch(u16 opcode) {
	u32 newAddress = reg.R[15] + ((i16)(opcode << 5) >> 4);
	fetchOpcode();

	reg.R[15] = newAddress;
	flushPipeline();
}

template <bool lowHigh>
void ARM7TDMI::thumbLongBranchLink(u16 opcode) {
	if constexpr (lowHigh) {
		u32 address = reg.R[14] + ((opcode & 0x7FF) << 1);
		reg.R[14] = (reg.R[15] - 2) | 1;
		fetchOpcode();

		reg.R[15] = address;
		flushPipeline();
	} else {
		reg.R[14] = reg.R[15] + ((i32)((u32)opcode << 21) >> 9);
		fetchOpcode();
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