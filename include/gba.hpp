
#ifndef GBA_H
#define GBA_H

#include <array>
#include <atomic>
#include <cstdint>
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

//#include <format>
#include <fmt/core.h>

using i8 = std::int8_t;
using u8 = std::uint8_t;
using i16 = std::int16_t;
using u16 = std::uint16_t;
using i32 = std::int32_t;
using u32 = std::uint32_t;
using i64 = std::int64_t;
using u64 = std::uint64_t;

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

	std::atomic<bool> running;
	std::atomic<bool> step;
	std::atomic<bool> trace;
	std::stringstream log;

	std::vector<u8> romBuff;

private:
	u8 *pageTableRead[8192];
	u8 *pageTableWrite[8192];
};

#endif