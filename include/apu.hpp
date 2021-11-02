
#ifndef GBA_APU
#define GBA_APU

#include "types.hpp"

class GameBoyAdvance;
class GBAAPU {
public:
	GameBoyAdvance& bus;

	GBAAPU(GameBoyAdvance& bus_);
	void reset();

    u8 readIO(u32 address);
	void writeIO(u32 address, u8 value);
};

#endif