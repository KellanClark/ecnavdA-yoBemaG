
#include "typedefs.hpp"
#include <array>
#include "gba.hpp"

GameBoyAdvance::GameBoyAdvance() : cpu(*this) {
	reset();
}

void GameBoyAdvance::reset() {
	running = false;
}

void GameBoyAdvance::run() {
	while (1) {
		//printf("%d\n", running);
		if (running) {
			printf("test\n");
			cpu.cycle();
		}
	}
}

// Stolen from emudev Discord
template <typename T>
std::vector<T> LoadBin(const std::filesystem::path& path) {
	std::basic_ifstream<T> file{path, std::ios::binary};
	return {std::istreambuf_iterator<T>{file}, {}};
}

int GameBoyAdvance::loadRom(std::filesystem::path romFilePath_, std::filesystem::path biosFilePath_) {
	running = false;
	//romBuff = LoadBin<u8>(romFilePath_);
	std::ifstream romFileStream;
	romFileStream.open(romFilePath_, std::ios::binary);
	if (!romFileStream.is_open()) {
		printf("Failed to open ROM!\n");
		return -1;
	}
	romFileStream.seekg(0, std::ios::end);
	size_t romSize = romFileStream.tellg();
	romFileStream.seekg(0, std::ios::beg);
	romFileStream.read(reinterpret_cast<char*>(romBuff.data()), romBuff.size());
	romFileStream.close();
	
	if (romBuff.size() > 0x02000000)
		romBuff.resize(0x02000000);
	{ // Round rom size to power of 2 https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
		u32 v = romBuff.size() - 1;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		romBuff.resize(v + 1);
	}
	// Fill page table for rom buffer
	if (romBuff.size() > 0x8000) {
		for (int pageIndex = 0; pageIndex < (romBuff.size() >> 15); pageIndex++) {
			u8 *ptr = &romBuff[pageIndex << 15];
			pageTableRead[pageIndex | 0x1000] = ptr; // 0x0800'0000 - 0x09FF'FFFF
			pageTableRead[pageIndex | 0x1400] = ptr; // 0x0A00'0000 - 0x0BFF'FFFF
			pageTableRead[pageIndex | 0x1800] = ptr; // 0x0C00'0000 - 0x0DFF'FFFF
		}
	}

	running = true;
	return 0;
}

void GameBoyAdvance::save() {
	//
}

template <typename T>
T GameBoyAdvance::read(u32 address) {
	int page = (address & 0x0FFFFFFF) >> 15;
	int offset = address & 0x7FFF;
	void *pointer = pageTableRead[page];

	if (pointer != NULL) {
		/*void *source;
		T val;
		std::memcpy (&val, source, sizeof(T));
		return val;*/
		return *(T*)(pointer + offset);
	} else {
		switch (page) {
		case 0x1000 ... 0x1BFF:
			if ((page == 0x1000) && (romBuff.size() < 0x8000) && (offset < romBuff.size())) {
				return *(T*)(&romBuff + offset);
			} else { // ROM open bus
				return 0;
			}
		default:
			return 0;
		}
	}
}

template u16 GameBoyAdvance::read<u16>(u32);
template u32 GameBoyAdvance::read<u32>(u32);

/*
u16 readHalfword(u32 address) {
	//
}

u32 readWord(u32 address) {
	//
}*/