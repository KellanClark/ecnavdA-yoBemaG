
#include "fmt/core.h"
#include "types.hpp"
#include "hlebios.hpp"
#include "cpu.hpp"
#include "gba.hpp"

#include <cmath>
#include <cstdio>

GBABIOS::GBABIOS(GBACPU& cpu_) : cpu(cpu_) {
	//
}

void GBABIOS::jumpToBios() {
	processJump = false;
	switch (cpu.reg.R[15] - (cpu.reg.thumbMode ? 4 : 8)) {
	case 0x0000: reset(); break;
	case 0x0008: enterSwi(); break;
	case 0x0018: enterInterrupt(); break;
	case 0x0138: exitInterrupt(); break;
	case 0x01B4: exitHalt(); break;
	case 0x0170: exitSwi(); break;
	case 0x0348: loopIntrWait(); break;
	default:
		printf("Unimplemented jump to BIOS location 0x%07X\n", cpu.reg.R[15] - (cpu.reg.thumbMode ? 4 : 8));
		cpu.bus.log << fmt::format("Unimplemented jump to BIOS location 0x{:07X}\n", cpu.reg.R[15] - (cpu.reg.thumbMode ? 4 : 8));
		cpu.running = false;
		break;
	}
}

void GBABIOS::reset() { // TODO: look into actual boot sequence
	RegisterRamReset(0xFF);
	SoftReset();
}

void GBABIOS::enterInterrupt() {
	cpu.tickScheduler(3);
	cpu.reg.R[15] = 0x128;
	cpu.blockDataTransfer<true, false, false, true, false>(0xE92D500F); // stmdb r13!, {r0-r3, r12, lr}
	cpu.reg.R[0] = 0x4000000; // mov r0, #0x4000000
	cpu.reg.R[14] = 0x138; // add lr, pc, #0
	cpu.tickScheduler(2);
	cpu.nextFetchType = true;
	cpu.singleDataTransfer<false, true, false, false, false, true>(0xE510F004); // ldr pc, [r0, #-4]
}

void GBABIOS::exitInterrupt() {
	cpu.blockDataTransfer<false, true, false, true, true>(0xE8BD500F); // ldmia r13!, {r0-r3, r12, lr}
	cpu.nextFetchType = true;
	cpu.dataProcessing<true, 0x2, true>(0xE25EF004); // subs pc, lr, #4
}

void GBABIOS::enterSwi() {
	int functionNum = cpu.bus.read<u8, false>(cpu.reg.R[14] - 2, false);
	if (functionNum > 0xF)
		return;

	cpu.reg.R[15] = 0x140; // b 0x140
	// stmdb r13!, {r11, r12, lr}
	cpu.bus.write(cpu.reg.R[13] - 12, cpu.reg.R[11], false);
	cpu.bus.write(cpu.reg.R[13] - 8, cpu.reg.R[12], true);
	cpu.bus.write(cpu.reg.R[13] - 4, cpu.reg.R[14], true);
	cpu.reg.R[13] -= 12;
	// mrs r11, spsr
	u32 oldSpsr = cpu.reg.SPSR_svc;
	// stmdb sp!, {r11}
	cpu.bus.write(cpu.reg.R[13] - 4, oldSpsr, false);
	cpu.reg.R[13] -= 4;
	// and r11, r11, #0x80; orr r11, r11, #0x1f; msr cpsr_fc, r11
	cpu.bankRegisters(GBACPU::MODE_SYSTEM, false); // Functions are run in system mode
	cpu.reg.CPSR = (oldSpsr & 0x80) | 0x1F;
	// stmdb r13!, {r2, lr}
	cpu.bus.write(cpu.reg.R[13] - 8, cpu.reg.R[2], false);
	cpu.bus.write(cpu.reg.R[13] - 4, cpu.reg.R[14], true);
	cpu.reg.R[13] -= 8;
	cpu.reg.R[14] = 0x0170; // adr lr, swi_complete
	cpu.tickScheduler(14);

	u32 arg0 = out0 = cpu.reg.R[0];
	u32 arg1 = out1 = cpu.reg.R[1];
	u32 arg2 = cpu.reg.R[2];
	u32 arg3 = out3 = cpu.reg.R[3];

	switch (functionNum) {
	case 0x00: SoftReset(); return;
	case 0x01: RegisterRamReset(arg0); break;
	case 0x02: Halt();
		if (cpu.processIrq) {
			cpu.reg.R[15] = 0x01B4 + 8;
			return;
		}
		break;
	case 0x03: Stop();
		if (cpu.processIrq) {
			cpu.reg.R[15] = 0x01B4 + 8;
			return;
		}
		break;
	case 0x04: IntrWait(arg0, arg1); return;
	case 0x05: VBlankIntrWait(); return;
	case 0x06: Div(arg0, arg1); break;
	case 0x07: DivArm(arg0, arg1); break;
	case 0x08: Sqrt(arg0); break;
	case 0x09: ArcTan(arg0, arg1); break;
	case 0x0A: ArcTan2(arg0, arg1); break;
	case 0x0B: CpuSet(arg0, arg1, arg2); break;
	case 0x0C: CpuFastSet(arg0, arg1, arg2); break;
	case 0x0D: GetBiosChecksum(); break;
	case 0x0E: BgAffineSet(arg0, arg1, arg2); break;
	case 0x0F: ObjAffineSet(arg0, arg1, arg2, arg3); break;
	default:
		printf("Unimplemented software interrupt 0x%02X\nr0: 0x%08X  r1: 0x%08X  r2: 0x%08X  r3: 0x%08X\n", functionNum, arg0, arg1, arg2, arg3);
		cpu.running = false;
		break;
	}

	cpu.reg.R[0] = out0;
	cpu.reg.R[1] = out1;
	cpu.reg.R[3] = out3;

	exitSwi();
}

void GBABIOS::exitSwi() {
	// ldmia r13!, {r2, lr}
	cpu.reg.R[2] = cpu.bus.read<u32, false, false>(cpu.reg.R[13], false);
	cpu.reg.R[14] = cpu.bus.read<u32, false, false>(cpu.reg.R[13] + 4, true);
	cpu.reg.R[13] += 8;
	// mov r12, #0xd3; msr cpsr_fc, r12
	cpu.bankRegisters(GBACPU::MODE_SUPERVISOR, false);
	cpu.reg.CPSR = 0xD3;
	// ldm sp!, {r11}; msr spsr_fc, r11
	cpu.reg.SPSR_svc = cpu.bus.read<u32, false, false>(cpu.reg.R[13], false);
	cpu.reg.R[13] += 4;
	// ldmia r13!, {r11, r12, lr}
	cpu.reg.R[11] = cpu.bus.read<u32, false, false>(cpu.reg.R[13], false);
	cpu.reg.R[12] = cpu.bus.read<u32, false, false>(cpu.reg.R[13] + 4, true);
	cpu.reg.R[14] = cpu.bus.read<u32, false, false>(cpu.reg.R[13] + 8, true);
	cpu.reg.R[13] += 12;
	// movs pc, lr
	cpu.reg.R[15] = cpu.reg.R[14];
	cpu.leaveMode();
	cpu.flushPipeline();
	cpu.tickScheduler(10);

	cpu.bus.biosOpenBusValue = 0xE3A02004;
}

const u16 sineTable[] = {
	0x0000, 0x0192, 0x0323, 0x04B5, 0x0645, 0x07D5, 0x0964, 0x0AF1, 0x0C7C, 0x0E05, 0x0F8C, 0x1111, 0x1294, 0x1413, 0x158F, 0x1708,
	0x187D, 0x19EF, 0x1B5D, 0x1CC6, 0x1E2B, 0x1F8B, 0x20E7, 0x223D, 0x238E, 0x24DA, 0x261F, 0x275F, 0x2899, 0x29CD, 0x2AFA, 0x2C21,
	0x2D41, 0x2E5A, 0x2F6B, 0x3076, 0x3179, 0x3274, 0x3367, 0x3453, 0x3536, 0x3612, 0x36E5, 0x37AF, 0x3871, 0x392A, 0x39DA, 0x3A82,
	0x3B20, 0x3BB6, 0x3C42, 0x3CC5, 0x3D3E, 0x3DAE, 0x3E14, 0x3E71, 0x3EC5, 0x3F0E, 0x3F4E, 0x3F84, 0x3FB1, 0x3FD3, 0x3FEC, 0x3FFB,
	0x4000, 0x3FFB, 0x3FEC, 0x3FD3, 0x3FB1, 0x3F84, 0x3F4E, 0x3F0E, 0x3EC5, 0x3E71, 0x3E14, 0x3DAE, 0x3D3E, 0x3CC5, 0x3C42, 0x3BB6,
	0x3B20, 0x3A82, 0x39DA, 0x392A, 0x3871, 0x37AF, 0x36E5, 0x3612, 0x3536, 0x3453, 0x3367, 0x3274, 0x3179, 0x3076, 0x2F6B, 0x2E5A,
	0x2D41, 0x2C21, 0x2AFA, 0x29CD, 0x2899, 0x275F, 0x261F, 0x24DA, 0x238E, 0x223D, 0x20E7, 0x1F8B, 0x1E2B, 0x1CC6, 0x1B5D, 0x19EF,
	0x187D, 0x1708, 0x158F, 0x1413, 0x1294, 0x1111, 0x0F8C, 0x0E05, 0x0C7C, 0x0AF1, 0x0964, 0x07D5, 0x0645, 0x04B5, 0x0323, 0x0192,
	0x0000, 0xFE6E, 0xFCDD, 0xFB4B, 0xF9BB, 0xF82B, 0xF69C, 0xF50F, 0xF384, 0xF1FB, 0xF074, 0xEEEF, 0xED6C, 0xEBED, 0xEA71, 0xE8F8,
	0xE783, 0xE611, 0xE4A3, 0xE33A, 0xE1D5, 0xE075, 0xDF19, 0xDDC3, 0xDC72, 0xDB26, 0xD9E1, 0xD8A1, 0xD767, 0xD633, 0xD506, 0xD3DF,
	0xD2BF, 0xD1A6, 0xD095, 0xCF8A, 0xCE87, 0xCD8C, 0xCC99, 0xCBAD, 0xCACA, 0xC9EE, 0xC91B, 0xC851, 0xC78F, 0xC6D6, 0xC626, 0xC57E,
	0xC4E0, 0xC44A, 0xC3BE, 0xC33B, 0xC2C2, 0xC252, 0xC1EC, 0xC18F, 0xC13B, 0xC0F2, 0xC0B2, 0xC07C, 0xC04F, 0xC02D, 0xC014, 0xC005,
	0xC000, 0xC005, 0xC014, 0xC02D, 0xC04F, 0xC07C, 0xC0B2, 0xC0F2, 0xC13B, 0xC18F, 0xC1EC, 0xC252, 0xC2C2, 0xC33B, 0xC3BE, 0xC44A,
	0xC4E0, 0xC57E, 0xC626, 0xC6D6, 0xC78F, 0xC851, 0xC91B, 0xC9EE, 0xCACA, 0xCBAD, 0xCC99, 0xCD8C, 0xCE87, 0xCF8A, 0xD095, 0xD1A6,
	0xD2BF, 0xD3DF, 0xD506, 0xD633, 0xD767, 0xD8A1, 0xD9E1, 0xDB26, 0xDC72, 0xDDC3, 0xDF19, 0xE075, 0xE1D5, 0xE33A, 0xE4A3, 0xE611,
	0xE783, 0xE8F8, 0xEA71, 0xEBED, 0xED6C, 0xEEEF, 0xF074, 0xF1FB, 0xF384, 0xF50F, 0xF69C, 0xF82B, 0xF9BB, 0xFB4B, 0xFCDD, 0xFE6E
};

void GBABIOS::SoftReset() { // 0x00
	u8 multiboot = cpu.bus.read<u8, false>(0x4000000 - 6, false);

	// Reset and clear stack
	cpu.reg.R13_svc = 0x03008000 - 0x20;
	cpu.reg.R14_svc = 0;
	cpu.reg.SPSR_svc = 0;
	cpu.reg.R13_irq = 0x03008000 - 0x60;
	cpu.reg.R14_irq = 0;
	cpu.reg.SPSR_irq = 0;
	cpu.reg.R[13] = 0x03008000 - 0x100;
	for (i32 offset = 0xFFFFFE00; offset < 0; offset += 4)
		cpu.bus.write<u32>(0x4000000 + offset, 0, false);
	
	for (int i = 0; i <= 13; i++)
		cpu.reg.R[i] = 0;
	cpu.reg.R[14] = multiboot ? 0x2000000 : 0x8000000;
	cpu.reg.CPSR = 0x1F;
	// bx lr
	cpu.tickScheduler(1);
	cpu.reg.R[15] = cpu.reg.R[14];
	cpu.flushPipeline();
	cpu.bus.biosOpenBusValue = 0xE129F000;
}

void GBABIOS::RegisterRamReset(u32 flags) { // 0x01
	cpu.bus.write<u32>(cpu.reg.R[13] -= (4 + 20), 0, false);
	cpu.bus.write<u16>(0x4000000, 0x0080, false); // DISPCNT

	if (flags & 0x80) { // I/O
		CpuFastSet(cpu.reg.R[13], 0x4000200, 8 | 0x85000000);
		cpu.bus.write<u16>(0x4000202, 0xFFFF, false);
		cpu.bus.write<u8>(0x4000410, 0xFF, false); // Known bug in this function
		CpuFastSet(cpu.reg.R[13], 0x4000004, 8 | 0x85000000); // DISPSTAT - BG2PD
		CpuFastSet(cpu.reg.R[13], out1 - 4, 0x10 | 0x85000000); // BG2PA - BLDY
		CpuFastSet(cpu.reg.R[13], 0x40000B0, 0x18 | 0x85000000); // DMA and timer I/O
		cpu.bus.write<u32>(out1 + 0x20, 0, false); // KEYINPUT/KEYCNT
		cpu.bus.write<u16>(0x4000020, 0x0100, false); // BG2PA
		cpu.bus.write<u16>(0x4000030, 0x0100, false); // BG2PD
		cpu.bus.write<u16>(0x4000026, 0x0100, false); // BG3PA
		cpu.bus.write<u16>(0x4000036, 0x0100, false); // BG3PD
	}

	if (flags & 0x20) { // Serial
		CpuFastSet(cpu.reg.R[13], 0x4000110, 8 | 0x85000000); // Serial Communication (1)
		cpu.bus.write<u16>(out1 + 4, 0x8000, false); // RCNT
		cpu.bus.write<u16>(out1 += 0x10, 7, false); // JOYCNT
		CpuFastSet(cpu.reg.R[13], 0x4000110, 7 | 0x85000000);
	} else {
		cpu.bus.write<u16>(0x4000114, 0x8000, false); // bug?
		cpu.bus.write<u16>(out1 += 0x10, 7, false); // SIOMULTI2
	}

	if (flags & 0x40) { // Sound
		out1 = 0x4000080;
		cpu.bus.write<u8>(out1 + 4, (u8)0x880E0000, false); // SOUNDCNT_X
		cpu.bus.write<u8>(out1 + 4, (u8)out1, false); // SOUNDCNT_X
		cpu.bus.write<u32>(out1, 0x880E0000, false); // SOUNDCNT_L/SOUNDCNT_H
		cpu.bus.write<u16>(out1 + 8, cpu.bus.read<u16, false>(out1 + 8, false), false); // SOUNDBIAS(wtf) 
		out1 -= 0x10;
		cpu.bus.write<u8>(out1, (u8)out1, false); // SOUND3CNT_L
		CpuFastSet(cpu.reg.R[13], out1 += 0x20, 8 | 0x85000000); // Wave RAM
		cpu.bus.write<u8>(out1 -= 0x40, 0, false); // SOUND3CNT_L
		CpuFastSet(cpu.reg.R[13], out1 += 0x20, 8 | 0x85000000); // Wave RAM
		out1 = 0x4000080;
		cpu.bus.write<u8>(out1 + 4, 0, false); // SOUNDCNT_X
	}

	if (flags & 0x01) // EWRAM
		CpuFastSet(cpu.reg.R[13], 0x2000000, 0x10000 | 0x85000000);
	
	if (flags & 0x08) // VRAM
		CpuFastSet(cpu.reg.R[13], 0x6000000, 0x6000 | 0x85000000);
	
	if (flags & 0x10) // OAM
		CpuFastSet(cpu.reg.R[13], 0x7000000, 0x100 | 0x85000000);
	
	if (flags & 0x04) // Palette
		CpuFastSet(cpu.reg.R[13], 0x5000000, 0x100 | 0x85000000);
	
	if (flags & 0x02) { // IWRAM
		CpuFastSet(cpu.reg.R[13], 0x3000000, 0x1F80 | 0x85000000);
	} else {
		out0 = cpu.reg.R[13];
		out1 = 0x3000000;
		cpu.reg.R[2] = 0x1F80 | 0x85000000;
	}

	cpu.reg.R[13] += 4 + 20;
	out3 = 0x00000170;
}

void GBABIOS::Halt() { // 0x02
	cpu.reg.R[2] = 0;
	cpu.reg.R[12] = 0x4000000;
	cpu.tickScheduler(6);

	cpu.bus.write<u8>(0x4000301, 0, false); // HALTCNT
	while (!cpu.halted) {
		cpu.currentTime = cpu.eventQueue.top().timeStamp;
		cpu.tickScheduler(1);
	}
}

void GBABIOS::Stop() { // 0x03
	cpu.reg.R[2] = 0x80;
	cpu.reg.R[12] = 0x4000000;
	cpu.tickScheduler(3);

	cpu.bus.write<u8>(0x4000301, 0x80, false); // HALTCNT
	while (!cpu.stopped) {
		cpu.currentTime = cpu.eventQueue.top().timeStamp;
		cpu.tickScheduler(1);
	}
}

void GBABIOS::exitHalt() {
	cpu.tickScheduler(3);
	exitSwi();
}

void GBABIOS::IntrWait(bool discardOldFlags, u16 wantedFlags) { // 0x04
	cpu.reg.R[0] = discardOldFlags;
	cpu.reg.R[1] = wantedFlags;
	cpu.blockDataTransfer<true, false, false, true, false>(0xE92D4010); // stmdb r13!, {r4, lr}
	cpu.reg.R[3] = 0; // mov r3, #0
	cpu.reg.R[4] = 1; // mov r4, #1

	if (discardOldFlags) { // cmp r0, #0; blne sub_0358
		cpu.IME = false; // strb r3, [r12, #0x208]
		u16 tmp2 = cpu.bus.read<u16, false>(0x4000000 - 8, false); // ldrh r2, [r12, #-8]
		if (wantedFlags & tmp2) { // ands r0, r1, r2
			cpu.bus.write<u16>(0x4000000 - 8, tmp2 ^ (wantedFlags & tmp2), false); // eorne r2, r2, r0; strhne r2, [r12, #-8]
		}
		cpu.IME = true; // strb r4, [r12, #0x208]
	}

	cpu.bus.write<u8>(0x4000301, 0, false); // strb r3, [r12, #0x301]
	while (!cpu.processIrq) {
		cpu.currentTime = cpu.eventQueue.top().timeStamp;
		cpu.tickScheduler(1);
	}
	cpu.reg.R[15] = 0x0348 + 8;
	return;
}

void GBABIOS::loopIntrWait() {
	u16 tmp2 = cpu.bus.read<u16, false>(0x4000000 - 8, false); // ldrh r2, [r12, #-8]
	if (cpu.reg.R[1] & tmp2) { // ands r0, r1, r2
		cpu.bus.write<u16>(0x4000000 - 8, tmp2 ^ (cpu.reg.R[1] & tmp2), false); // eorne r2, r2, r0; strhne r2, [r12, #-8]
		cpu.bus.write(0x4000208, cpu.reg.R[4], false); // strb r4, [r12, #0x208]

		cpu.blockDataTransfer<false, true, false, true, true>(0xE8BD4010); // ldmia r13!, {r4, lr}
		cpu.branchExchange(0xE12FFF1E); // bx lr
	} else { // beq _0344
		cpu.bus.write(0x4000208, cpu.reg.R[4], false); // strb r4, [r12, #0x208]
		
		cpu.bus.write<u8>(0x4000301, 0, false); // strb r3, [r12, #0x301]
		while (!cpu.processIrq) {
			cpu.currentTime = cpu.eventQueue.top().timeStamp;
			cpu.tickScheduler(1);
		}
		cpu.reg.R[15] = 0x0348 + 8;
	}
}

void GBABIOS::VBlankIntrWait() { // 0x05
	cpu.tickScheduler(2);
	IntrWait(true, 0x0001);
}

void GBABIOS::Div(i32 numerator, i32 denominator) { // 0x06
	bool numeratorSign = numerator < 0;
	bool resultSign = (denominator < 0) ^ numeratorSign;
	numerator = std::abs(numerator);
	denominator = std::abs(denominator);

	if (denominator == 0) { [[unlikely]]
		out1 = numerator;
		out0 = out3 = 1;
	} else {
		out1 = numerator % denominator;
		out0 = out3 = numerator / denominator;
	}

	if (resultSign) out0 *= -1;
	if (numeratorSign) out1 *= -1;
}

void GBABIOS::DivArm(i32 denominator, i32 numerator) { // 0x07
	cpu.tickScheduler(3);
	Div(numerator, denominator);
}

void GBABIOS::Sqrt(u32 operand) { // 0x08
	out0 = sqrt(operand);
}

void GBABIOS::ArcTan(i32 tan, i32 tmp1) { // 0x09
	tmp1 = tan * tan; // mul r1, r0, r0
	tmp1 >>= 0xE; // asr r1, r1, #0xe
	tmp1 = 0 - tmp1; // rsb r1, r1, #0
	i32 tmp3 = 0xA9; // mov r3, #0xa9
	tmp3 = tmp1 * tmp3; // mul r3, r1, r3
	tmp3 >>= 0xE; // asr r3, r3, #0xe
	tmp3 += 0x390; // add r3, r3, #0x390
	tmp3 = tmp1 * tmp3; // mul r3, r1, r3
	tmp3 >>= 0xE; // asr r3, r3, #0xe
	tmp3 += 0x91C; // add r3, r3, #0x900; add r3, r3, #0x1c
	tmp3 = tmp1 * tmp3; // mul r3, r1, r3
	tmp3 >>= 0xE; // asr r3, r3, #0xe
	tmp3 += 0xFB6; // add r3, r3, #0xf00; add r3, r3, #0xb6
	tmp3 = tmp1 * tmp3; // mul r3, r1, r3
	tmp3 >>= 0xE; // asr r3, r3, #0xe
	tmp3 += 0x16AA; // add r3, r3, #0x1600; add r3, r3, #0xaa
	tmp3 = tmp1 * tmp3; // mul r3, r1, r3
	tmp3 >>= 0xE; // asr r3, r3, #0xe
	tmp3 += 0x2081; // add r3, r3, #0x2000; add r3, r3, #0x81
	tmp3 = tmp1 * tmp3; // mul r3, r1, r3
	tmp3 >>= 0xE; // asr r3, r3, #0xe
	tmp3 += 0x3651; // add r3, r3, #0x3600; add r3, r3, #0x51
	tmp3 = tmp1 * tmp3; // mul r3, r1, r3
	tmp3 >>= 0xE; // asr r3, r3, #0xe
	tmp3 += 0xA2F9; // add r3, r3, #0xa200; add r3, r3, #0xf9
	tan = tmp3 * tan; // mul r0, r3, r0
	tan >>= 0x10; // asr r0, r0, #0x10

	out0 = tan;
	out1 = tmp1;
	out3 = tmp3;
}

inline void GBABIOS::ArcTan2(i32 x, i32 y) { // 0x0A
	if (y != 0) { // cmp r1, #0; bne _0510
		if (x != 0) { // cmp r0, #0; bne _0524
			i32 tmp2 = x << 0xE; // adds r2, r0, #0; lsls r2, r2, #0xe
			i32 tmp3 = y << 0xE; // adds r3, r1, #0; lsls r3, r3, #0xe
			i32 negX = 0 - x; // negs r4, r0
			i32 negY = 0 - y; // negs r5, r1
			i32 tmp6 = 0x4000; // movs r6, #0x40; lsls r6, r6, #8
			i32 tmp7 = 0x8000; // lsls r7, r6, #1
			// Fuck it. I'll use gotos.
			if (y < 0) { // cmp r1, #0; blt _0572
				if (x > 0) { // cmp r0, #0; bgt _058A
					if (x < negY) // cmp r0, r5
						goto _057A; // blt _057A
					Div(tmp3, x); // adds r1, r0, #0; adds r0, r3, #0; bl swi_Div_t
					ArcTan(out0, out1); // bl swi_ArcTan_t
					out0 = tmp7 + tmp7 + out0; // adds r7, r7, r7; adds r0, r7, r0
				} else if (negX > negY) { // cmp r4, r5
					goto _0562; // bgt _0562
				} else {
					_057A:
					Div(tmp2, y); // adds r0, r2, #0; bl swi_Div_t
					ArcTan(out0, out1); // bl swi_ArcTan_t
					out0 = (tmp6 + tmp7) - out0; // adds r6, r6, r7; subs r0, r6, r0
				}
			} else if (x < 0) { // cmp r0, #0; blt _055E
				if (negX < y) // cmp r4, r1
					goto _0550; // blt _0550

				_0562:
				Div(tmp3, x); // adds r1, r0, #0; adds r0, r3, #0; bl swi_Div_t
				ArcTan(out0, out1); // bl swi_ArcTan_t
				out0 += tmp7; // adds r0, r7, r0
			} else if (x < y) { // cmp r0, r1; blt _0550
				_0550:
				Div(tmp2, y); // adds r0, r2, #0; bl swi_Div_t
				ArcTan(out0, out1); // bl swi_ArcTan_t
				out0 = tmp6 - out0; // subs r0, r6, r0
			} else {
				Div(tmp3, x); // adds r1, r0, #0; adds r0, r3, #0; bl swi_Div_t
				ArcTan(out0, out1); // bl swi_ArcTan_t
			}
		} else if (y < 0) { // cmp r1, #0; blt _051E
			// Return 3pi/2
			out0 = 0xC000; // movs r0, #0xc0; lsls r0, r0, #8
		} else {
			// Return pi/2
			out0 = 0x4000; // movs r0, #0x40; lsls r0, r0, #8
		}
	} else if (x < 0) { // cmp r0, #0; blt _050A
		// Return pi
		out0 = 0x8000; // movs r0, #0x80; lsls r0, r0, #8
	} else {
		// Return 0
		out0 = 0; // movs r0, #0
	}

	out3 = 0x00000170;
}

void GBABIOS::CpuSet(u32 srcAddress, u32 dstAddress, u32 lengthMode) { // 0x0B
	u32 size = (lengthMode << 11) >> 9;
	if ((size == 0) || !((((size & ~0xFE000000) + srcAddress) | srcAddress) & 0xE000000))
		return;

	if ((lengthMode >> 26) & 1) { // 32 bit
		u32 endAddress = dstAddress + size;
		if ((lengthMode >> 24) & 1) { // Fixed source address
			u32 value = cpu.bus.read<u32, false, false>(srcAddress, false);
			srcAddress += 4;

			for (; dstAddress < endAddress; dstAddress += 4)
				cpu.bus.write<u32>(dstAddress, value, false);
		} else {
			for (; dstAddress < endAddress; srcAddress += 4, dstAddress += 4)
				cpu.bus.write<u32>(dstAddress, cpu.bus.read<u32, false, false>(srcAddress, false), false);
		}
	} else { // 16 bit
		u32 offset = 0;
		if ((lengthMode >> 24) & 1) { // Fixed source address
			u16 value = cpu.bus.read<u16, false>(srcAddress, false);

			for (; offset < size; offset += 2)
				cpu.bus.write<u16>(dstAddress + offset, value, false);
		} else {
			for (; offset < size; offset += 2)
				cpu.bus.write<u16>(dstAddress + offset, cpu.bus.read<u16, false>(srcAddress + offset, false), false);
		}
	}

	out0 = srcAddress;
	out1 = dstAddress;
}

void GBABIOS::CpuFastSet(u32 srcAddress, u32 dstAddress, u32 lengthMode) { // 0x0C
	cpu.tickScheduler(37);

	u32 size = (lengthMode << 11) >> 9;
	if (size == 0)
		return;
	if (!((((size & ~0xFE000000) + srcAddress) | srcAddress) & 0xE000000)) {
		cpu.tickScheduler(2);
		return;
	}
	cpu.tickScheduler(9);

	u32 endAddress = size + dstAddress;
	if ((lengthMode >> 24) & 1) { // Fixed source address
		u32 value = cpu.bus.read<u32, false>(srcAddress, false);

		for (; dstAddress < endAddress; dstAddress += 32) {
			for (int i = 0; i < 32; i += 4)
				cpu.bus.write<u32>(dstAddress + i, value, (bool)i);
		}
		cpu.reg.R[2] = value;
		out3 = value;
	} else {
		for (; dstAddress < endAddress; srcAddress += 32, dstAddress += 32) {
			cpu.reg.R[2] = cpu.bus.read<u32, false, false>(srcAddress, false);
			cpu.bus.write<u32>(dstAddress, cpu.reg.R[2], false);
			out3 = cpu.bus.read<u32, false, false>(srcAddress + 4, true);
			cpu.bus.write<u32>(dstAddress + 4, out3, true);
			for (int i = 8; i < 32; i += 4)
				cpu.bus.write<u32>(dstAddress + i, cpu.bus.read<u32, false, false>(srcAddress + i, true), true);
		}
	}

	out0 = srcAddress;
	out1 = dstAddress;
}

void GBABIOS::GetBiosChecksum() { // 0x0D
	out0 = 0xBAAE187F;
}

void GBABIOS::BgAffineSet(u32 srcAddress, u32 dstAddress, u32 count) { // 0xE
	for (u32 i = 0; i < count; i++) {
		u16 rotateAngle = cpu.bus.read<u16, false>(srcAddress + 16, false); // ldrh r3, [r0, #0x10]
		rotateAngle >>= 8; // lsr r3, r3, #8
		i16 cosine = sineTable[(rotateAngle + 0x40) & 0xFF]; // add r8, r3, #0x40; and r8, r8, #0xff; lsl r8, r8, #1; ldrsh r11, [r8, r12]
		i16 sine = sineTable[rotateAngle]; // lsl r8, r3, #1; ldrsh r12, [r8, r12]
		i16 scaleX = cpu.bus.read<u16, false>(srcAddress + 12, false); // ldrsh r9, [r0, #0xc]
		i16 scaleY = cpu.bus.read<u16, false>(srcAddress + 14, false); // ldrsh r10, [r0, #0xe]
		i16 pa = (cosine * scaleX) >> 0xE; // mul r8, r11, r9; asr r3, r8, #0xe
		i16 pb = (sine * scaleX) >> 0xE; // mul r8, r12, r9; asr r4, r8, #0xe
		i16 pc = (sine * scaleY) >> 0xE; // mul r8, r12, r10; asr r5, r8, #0xe
		i16 pd = (cosine * scaleY) >> 0xE; // mul r8, r11, r10; asr r6, r8, #0xe
		i32 originalCenterX = cpu.bus.read<u32, false>(srcAddress, false); // ldm r0, {r9, r10, r12}
		i32 originalCenterY = cpu.bus.read<u32, false>(srcAddress + 4, false);
		i32 displayCenter = cpu.bus.read<u32, false>(srcAddress + 8, false);
		i16 displayCenterX = (displayCenter << 16) >> 16; // lsl r11, r12, #0x10; asr r11, r11, #0x10
		i16 displayCenterY = displayCenter >> 16; // asr r12, r12, #0x10
		i32 startX = (pb * displayCenterY) + ((pa * (0 - displayCenterX)) + originalCenterX); // rsb r8, r11, #0; mla r9, r3, r8, r9; mla r8, r4, r12, r9
		cpu.bus.write<u32>(dstAddress + 8, startX, false); // str r8, [r1, #8]
		i32 startY = (pd * (0 - displayCenterY)) + (pc * (0 - displayCenterX)) + originalCenterY; // rsb r8, r11, #0; mla r10, r5, r8, r10; rsb r8, r12, #0; mla r8, r6, r8, r10
		cpu.bus.write<u32>(dstAddress + 12, startY, false); // str r8, [r1, #0xc]
		cpu.bus.write<u16>(dstAddress, pa, false); // strh r3, [r1]
		pb = 0 - pb; // rsb r4, r4, #0
		cpu.bus.write<u16>(dstAddress + 2, pb, false); // strh r4, [r1, #2]
		cpu.bus.write<u16>(dstAddress + 4, pc, false); // strh r5, [r1, #4]
		cpu.bus.write<u16>(dstAddress + 6, pd, false); // strh r6, [r1, #6]
		srcAddress += 20; // add r0, r0, #0x14
		dstAddress += 16; // add r1, r1, #0x10
	}

	out0 = srcAddress;
	out1 = dstAddress;
}

void GBABIOS::ObjAffineSet(u32 srcAddress, u32 dstAddress, u32 count, u32 offset) { // 0xF
	for (u32 i = 0; i < count; i++) {
		u16 rotateAngle = cpu.bus.read<u16, false>(srcAddress + 4, false); // ldrh r9, [r0, #4]
		rotateAngle >>= 8; // lsr r9, r9, #8
		i16 cosine = sineTable[(rotateAngle + 0x40) & 0xFF]; // add r8, r9, #0x40; and r8, r8, #0xff; lsl r8, r8, #1; ldrsh r11, [r8, r12]
		i16 sine = sineTable[rotateAngle]; // lsl r8, r9, #1; ldrsh r12, [r8, r12]
		i16 scaleX = cpu.bus.read<u16, false>(srcAddress, false); // ldrsh r9, [r0]
		i16 scaleY = cpu.bus.read<u16, false>(srcAddress + 2, false); // ldrsh r10, [r0, #2]

		i16 pa = (cosine * scaleX) >> 0xE; // mul r8, r11, r9; asr r8, r8, #0xe
		cpu.bus.write<u16>(dstAddress, pa, false); // strh r8, [r1], r3
		dstAddress += offset;
		i16 pb = 0 - ((sine * scaleX) >> 0xE); // tmp8 = mul r8, r12, r9, asr r8, r8, #0xe; rsb r8, r8, #0
		cpu.bus.write<u16>(dstAddress, pb, false); // strh r8, [r1], r3
		dstAddress += offset;
		i16 pc = (sine * scaleY) >> 0xE; // mul r8, r12, r10; asr r8, r8, #0xe
		cpu.bus.write<u16>(dstAddress, pc, false); // strh r8, [r1], r3
		dstAddress += offset;
		i16 pd = (cosine * scaleY) >> 0xE; // mul r8, r11, r10; asr r8, r8, #0xe
		cpu.bus.write<u16>(dstAddress, pd, false); // strh r8, [r1], r3
		dstAddress += offset;
		srcAddress += 8;
	}

	out0 = srcAddress;
	out1 = dstAddress;
}