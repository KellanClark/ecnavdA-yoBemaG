
#ifndef GBA_ARM7TDMI_HPP
#define GBA_ARM7TDMI_HPP

#include <array>
#include <sstream>
#include <string>

#include "types.hpp"

class GameBoyAdvance;
class ARM7TDMI {
public:
	GameBoyAdvance& bus;

	ARM7TDMI(GameBoyAdvance& bus_);
	void resetARM7TDMI();
	void cycle();

	u16 IE;
	u16 IF;
	bool IME;
	bool halted;

	std::string disassemble(u32 address, u32 opcode, bool thumb);
	std::string getRegName(unsigned int regNumber);
	std::string disassembleShift(u32 opcode, bool showUpDown);
	struct {
		bool showALCondition;
		bool alwaysShowSBit;
		bool printOperandsHex;
		bool printAddressesHex;
		bool simplifyRegisterNames;
		bool simplifyPushPop;
		bool ldmStmStackSuffixes;
	} disassemblerOptions;

	enum cpuMode {
		MODE_USER = 0x10,
		MODE_FIQ = 0x11,
		MODE_IRQ = 0x12,
		MODE_SUPERVISOR = 0x13,
		MODE_ABORT = 0x17,
		MODE_UNDEFINED = 0x1B,
		MODE_SYSTEM = 0x1F
	};
	struct {
		// Normal registers
		u32 R[16];

		union {
			struct {
				u32 mode : 5;
				u32 thumbMode : 1;
				u32 fiqDisable : 1;
				u32 irqDisable : 1;
				u32 : 20;
				u32 flagV : 1;
				u32 flagC : 1;
				u32 flagZ : 1;
				u32 flagN : 1;
			};
			u32 CPSR;
		};

		// Banked registers for each mode
		u32 R8_user, R9_user, R10_user, R11_user, R12_user, R13_user, R14_user;
		u32 R8_fiq, R9_fiq, R10_fiq, R11_fiq, R12_fiq, R13_fiq, R14_fiq, SPSR_fiq;
		u32 R13_svc, R14_svc, SPSR_svc;
		u32 R13_abt, R14_abt, SPSR_abt;
		u32 R13_irq, R14_irq, SPSR_irq;
		u32 R13_und, R14_und, SPSR_und;
	} reg;

	/* Instruction Decoding/Executing */
	int pipelineStage;
	u32 pipelineOpcode1; // R15
	u32 pipelineOpcode2; // R15 + 4
	u32 pipelineOpcode3; // R15 + 8
	bool incrementR15;
	bool tmpIncrement;

	inline bool checkCondition(int condtionCode);
	void unknownOpcodeArm(u32 opcode);
	void unknownOpcodeArm(u32 opcode, std::string message);
	void unknownOpcodeThumb(u16 opcode);
	void unknownOpcodeThumb(u16 opcode, std::string message);
	template <bool dataTransfer, bool iBit> bool computeShift(u32 opcode, u32 *result);

	void switchMode(cpuMode newMode);
	void bankRegisters(cpuMode newMode, bool changeCPSR);
	void leaveMode();

	template <bool iBit, int operation, bool sBit> void dataProcessing(u32 opcode);
	template <bool accumulate, bool sBit> void multiply(u32 opcode);
	template <bool signedMul, bool accumulate, bool sBit> void multiplyLong(u32 opcode);
	template <bool byteWord> void singleDataSwap(u32 opcode);
	template <bool targetPSR> void psrLoad(u32 opcode);
	template <bool targetPSR> void psrStoreReg(u32 opcode);
	template <bool targetPSR> void psrStoreImmediate(u32 opcode);
	void branchExchange(u32 opcode);
	template <bool prePostIndex, bool upDown, bool immediateOffset, bool writeBack, bool loadStore, int shBits> void halfwordDataTransfer(u32 opcode);
	template <bool immediateOffset, bool prePostIndex, bool upDown, bool byteWord, bool writeBack, bool loadStore> void singleDataTransfer(u32 opcode);
	void undefined(u32 opcode);
	template <bool prePostIndex, bool upDown, bool sBit, bool writeBack, bool loadStore> void blockDataTransfer(u32 opcode);
	template <bool lBit> void branch(u32 opcode);
	void softwareInterrupt(u32 opcode);

	template <int op, int shiftAmount> void thumbMoveShiftedReg(u16 opcode);							// 1
	template <bool immediate, bool op, int offset> void thumbAddSubtract(u16 opcode);					// 2
	template <int op, int destinationReg> void thumbAluImmediate(u16 opcode);							// 3
	template <int op> void thumbAluReg(u16 opcode);														// 4
	template <int op, bool opFlag1, bool opFlag2> void thumbHighRegOperation(u16 opcode);				// 5
	template <int destinationReg> void thumbPcRelativeLoad(u16 opcode);									// 6
	template <bool loadStore, bool byteWord, int offsetReg> void thumbLoadStoreRegOffset(u16 opcode);	// 7
	template <int hsBits, int offsetReg> void thumbLoadStoreSext(u16 opcode);							// 8
	template <bool byteWord, bool loadStore, int offset> void thumbLoadStoreImmediateOffset(u16 opcode);// 9
	template <bool loadStore, int offset> void thumbLoadStoreHalfword(u16 opcode);						// 10
	template <bool loadStore, int destinationReg> void thumbSpRelativeLoadStore(u16 opcode);			// 11
	template <bool spPc, int destinationReg> void thumbLoadAddress(u16 opcode);							// 12
	template <bool isNegative> void thumbSpAddOffset(u16 opcode);										// 13
	template <bool loadStore, bool pcLr> void thumbPushPopRegisters(u16 opcode);						// 14
	template <bool loadStore, int baseReg> void thumbMultipleLoadStore(u16 opcode);						// 15
	template <int condition> void thumbConditionalBranch(u16 opcode);									// 16
	void thumbSoftwareInterrupt(u16 opcode);															// 17
	void thumbUncondtionalBranch(u16 opcode);															// 18
	template <bool lowHigh> void thumbLongBranchLink(u16 opcode);										// 19

	static const std::array<void (ARM7TDMI::*)(u32), 4096> LUT;
	static const std::array<void (ARM7TDMI::*)(u16), 1024> thumbLUT;
};

#endif