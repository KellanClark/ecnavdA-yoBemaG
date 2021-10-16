
#ifndef GBA_HPP
#define GBA_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <functional>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <fmt/core.h>

#include "types.hpp"
#include "scheduler.hpp"
#include "cpu.hpp"
#include "ppu.hpp"

class GBACPU;
class GBAPPU;
class GameBoyAdvance {
public:
	GBACPU cpu;
	GBAPPU ppu;

	GameBoyAdvance();
	~GameBoyAdvance();
	void reset();

	int loadRom(std::filesystem::path romFilePath_, std::filesystem::path biosFilePath_);
	void save();

	template <typename T> T read(u32 address);
	template <typename T> void write(u32 address, T value);

	std::stringstream log;

	std::vector<u8> biosBuff;
	u8 ewram[0x40000];
	u8 iwram[0x8000];
	u16 KEYINPUT;
	std::vector<u8> romBuff;

private:
	u8 *pageTableRead[8192];
	u8 *pageTableWrite[8192];
};

#endif