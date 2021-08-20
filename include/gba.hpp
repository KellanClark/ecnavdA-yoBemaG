
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

	bool running;

private:
};

#endif