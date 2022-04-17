#ifndef GBA_BIOS_HPP
#define GBA_BIOS_HPP

#include "types.hpp"

class GBACPU;
class GBABIOS {
public:
	GBACPU& cpu;

	GBABIOS(GBACPU& cpu_);
	bool processJump;
	void jumpToBios();

	void reset();
	void enterInterrupt();
	void exitInterrupt();
	void enterSwi();
	void exitSwi();

private:
	// SWI functions
	u32 out0, out1, out3;
	void SoftReset(); // 0x00
	void RegisterRamReset(u32 flags); // 0x01
	void Halt(); // 0x02
	void Stop(); // 0x03
	void IntrWait(bool discardOldFlags, u16 wantedFlags); // 0x04
	void VBlankIntrWait(); // 0x05
	void Div(i32 numerator, i32 denominator); // 0x06
	void DivArm(i32 denominator, i32 numerator); // 0x07
	void Sqrt(u32 operand); // 0x08
	void ArcTan(i32 tan, i32 tmp1); // 0x09
	void ArcTan2(i32 x, i32 y); // 0x0A
	void CpuSet(u32 srcAddress, u32 dstAddress, u32 lengthMode); // 0x0B
	void CpuFastSet(u32 srcAddress, u32 dstAddress, u32 lengthMode); // 0x0C
	void GetBiosChecksum(); // 0x0D
	void BgAffineSet(u32 srcAddress, u32 dstAddress, u32 count); // 0xE
	void ObjAffineSet(u32 srcAddress, u32 dstAddress, u32 count, u32 offset); // 0xF

	void exitHalt();
	void loopIntrWait();
};

#endif