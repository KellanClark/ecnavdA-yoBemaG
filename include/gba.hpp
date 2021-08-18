
#ifndef GBA_H
#define GBA_H

#include <stdint.h>
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

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

#include "arm7tdmi.hpp"

namespace GBA {
	void reset();
	void run();

	int loadRom(std::filesystem::path romFilePath_, std::filesystem::path biosFilePath_);
	void save();

	bool running = false;
};

#endif