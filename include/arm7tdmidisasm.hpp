#ifndef ARM7TDMI_DISASM_HPP
#define ARM7TDMI_DISASM_HPP

#include <array>
#include <sstream>
#include <string>

#include "types.hpp"

class ARM7TDMIDisasmbler {
public:
    std::string disassemble(u32 address, u32 opcode, bool thumb);
	void defaultSettings();

	struct {
		bool showALCondition;
		bool alwaysShowSBit;
		bool printOperandsHex;
		bool printAddressesHex;
		bool simplifyRegisterNames;
		bool simplifyPushPop;
		bool ldmStmStackSuffixes;
	} options;

private:
	std::string getRegName(unsigned int regNumber);
	std::string disassembleShift(u32 opcode, bool showUpDown);
};

extern ARM7TDMIDisasmbler disassembler; // TODO: This shouldn't be required

#endif