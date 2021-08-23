
#ifndef GBA_CPU_H
#define GBA_CPU_H

#include "typedefs.hpp"
#include <array>

class GameBoyAdvance;
class ARM7TDMI {
public:
	GameBoyAdvance& bus;

	ARM7TDMI(GameBoyAdvance& bus_);
	void reset();
	void cycle();

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
		u32 R8_fiq, R9_fiq, R10_fiq, R11_fiq, R12_fiq, R13_fiq, R14_fiq, R15_fiq, SPSR_fiq;
		u32 R13_svc, R14_svc, SPSR_svc;
		u32 R13_abt, R14_abt, SPSR_abt;
		u32 R13_irq, R14_irq, SPSR_irq;
		u32 R13_und, R14_und, SPSR_und;
	} reg;

	template <bool word> void helloWorld(u32 opcode);

private:
	/* Instruction Decoding/Executing */
	int pipelineStage;
	u32 pipelineOpcode1; // R15
	u32 pipelineOpcode2; // R15 + 4
	u32 pipelineOpcode3; // R15 + 8
	bool incrementR15;

	void unknownOpcodeArm(u32 opcode);
	inline bool checkCondition(int condtionCode);

	static const std::array<void (ARM7TDMI::*)(u32), 4096> LUT;
};

#endif