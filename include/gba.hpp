
#ifndef GBA_H
#define GBA_H

#include <array>
#include <cstring>
#include <iostream>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "typedefs.hpp"
#include "arm7tdmi.hpp"

class GameBoyAdvance {
public:
	ARM7TDMI cpu;

	GameBoyAdvance();
	void reset();
	void run();

	int loadRom(std::filesystem::path romFilePath_, std::filesystem::path biosFilePath_);
	void save();

	template <typename T> T read(u32 address);
	//u16 readHalfword(u32 address); // Address is shifted right 1 for alignment.
	//u32 readWord(u32 address); // Address is shifted right 2 for alignment.

	bool running;

	std::vector<u8> romBuff;

private:
	u8 *pageTableRead[8192];
	u8 *pageTableWrite[8192];
};

#endif