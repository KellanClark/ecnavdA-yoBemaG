
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
#include "apu.hpp"
#include "dma.hpp"
#include "ppu.hpp"
#include "timer.hpp"

class GBACPU;
class GBAPPU;
class GameBoyAdvance {
public:
	GBACPU cpu;
	GBAAPU apu;
	GBADMA dma;
	GBAPPU ppu;
	GBATIMER timer;

	GameBoyAdvance();
	~GameBoyAdvance();
	void reset();

	int loadRom(std::filesystem::path romFilePath_, std::filesystem::path biosFilePath_);
	void save();

	template <typename T> T read(u32 address);
	template <typename T> void write(u32 address, T value);

	std::stringstream log;

	enum {
		UNKNOWN,
		EEPROM_512B,
		EEPROM_8K,
		SRAM_32K,
		FLASH_64K,
		FLASH_128K
	} saveType;
	std::filesystem::path saveFilePath;
	enum {
		READY,
		CMD_1,
		CMD_2
	} flashState;

	std::vector<u8> biosBuff;
	u8 ewram[0x40000];
	u8 iwram[0x8000];
	u16 KEYINPUT;
	u16 KEYCNT;
	std::vector<u8> romBuff;
	std::vector<u8> sram;

private:
	u8 *pageTableRead[8192];
	u8 *pageTableWrite[8192];
};

#endif