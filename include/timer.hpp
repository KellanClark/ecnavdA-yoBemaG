
#ifndef GBA_TIMER
#define GBA_TIMER

#include "types.hpp"

class GameBoyAdvance;
class GBATIMER {
public:
	GameBoyAdvance& bus;

	GBATIMER(GameBoyAdvance& bus_);
	void reset();

    u8 readIO(u32 address);
	void writeIO(u32 address, u8 value);
};

#endif