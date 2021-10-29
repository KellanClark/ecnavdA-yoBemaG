
#include "gba.hpp"
#include "arm7tdmi.hpp"
#include "scheduler.hpp"
#include <cstdio>

#define toPage(x) (x >> 15)

GameBoyAdvance::GameBoyAdvance() : cpu(*this), ppu(*this) {
	// Fill page tables
	for (int i = toPage(0x2000000); i < toPage(0x3000000); i += (256 / 32)) {
		for (int j = i; j < (i + (256 / 32)); j++) {
			pageTableRead[j] = pageTableWrite[j] = &ewram[(j - i) << 15];
		}
	}
	for (int i = toPage(0x3000000); i < toPage(0x4000000); i++) {
		pageTableRead[i] = pageTableWrite[i] = &iwram[0];
	}
	for (int i = toPage(0x6000000); i < toPage(0x7000000); i += 4) {
		pageTableRead[i] = pageTableWrite[i] = &ppu.vram[0];
		pageTableRead[i + 1] = pageTableWrite[i + 1] = &ppu.vram[0x08000];
		pageTableRead[i + 2] = pageTableWrite[i + 2] = &ppu.vram[0x10000];
	}

	reset();
}

GameBoyAdvance::~GameBoyAdvance() {
	save();
}

void GameBoyAdvance::reset() {
	cpu.running = false;
	log.str("");

	memset(ewram, 0, sizeof(ewram));
	memset(iwram, 0, sizeof(iwram));
	KEYINPUT = 0x1FF;
	KEYCNT = 0;

	systemEvents.reset();
	cpu.reset();
	ppu.reset();
}

int GameBoyAdvance::loadRom(std::filesystem::path romFilePath_, std::filesystem::path biosFilePath_) {
	cpu.running = false;

	std::ifstream biosFileStream{biosFilePath_, std::ios::binary};
	if (!biosFileStream.is_open()) {
		printf("Failed to open BIOS file: %s\n", biosFilePath_.c_str());
		return -1;
	}
	biosFileStream.seekg(0, std::ios::end);
	size_t biosSize = biosFileStream.tellg();
	biosFileStream.seekg(0, std::ios::beg);
	biosBuff.resize(biosSize);
	biosFileStream.read(reinterpret_cast<char *>(biosBuff.data()), biosSize);
	biosFileStream.close();
	biosBuff.resize(0x4000);

	std::ifstream romFileStream{romFilePath_, std::ios::binary};
	if (!romFileStream.is_open()) {
		printf("Failed to open ROM file: %s\n", romFilePath_.c_str());
		return -1;
	}
	romFileStream.seekg(0, std::ios::end);
	size_t romSize = romFileStream.tellg();
	romFileStream.seekg(0, std::ios::beg);
	romBuff.resize(romSize);
	romFileStream.read(reinterpret_cast<char *>(romBuff.data()), romSize);
	romFileStream.close();
	
	romBuff.resize(0x2000000);
	{ // Round rom size to power of 2 https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
		u32 v = romSize - 1;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		romSize = v + 1;
	}

	// Fill open bus values
	for (int i = romSize; i < 0x2000000; i += 2) {
		// TODO
	}

	// Fill page table for rom buffer
	for (int pageIndex = 0; pageIndex < 0x400; pageIndex++) {
		u8 *ptr = &romBuff[pageIndex << 15];
		pageTableRead[pageIndex | 0x1000] = ptr; // 0x0800'0000 - 0x09FF'FFFF
	}

	cpu.running = true;
	return 0;
}

void GameBoyAdvance::save() {
	/*std::ofstream saveFileStream{"vram.bin", std::ios::binary | std::ios::trunc};
	saveFileStream.write(reinterpret_cast<const char*>(ppu.paletteRam), sizeof(ppu.paletteRam));
	saveFileStream.close();*/
}

template <typename T>
T GameBoyAdvance::read(u32 address) {
	int page = (address & 0x0FFFFFFF) >> 15;
	int offset = address & 0x7FFF;
	void *pointer = pageTableRead[page];

	T val;
	if (pointer != NULL) {
		std::memcpy(&val, (u8*)pointer + offset, sizeof(T));
		return val;
	} else {
		switch (page) {
		case toPage(0x0000000): // BIOS
			if (address <= 0x3FFF) {
				std::memcpy(&val, (u8*)biosBuff.data() + offset, sizeof(T));
				return val;
			}
			break;
		case toPage(0x4000000): // I/O
			// Split everything into u8
			if (sizeof(T) == 4) {
				u32 val = read<u8>(address++);
				val |= read<u8>(address++) << 8;
				val |= read<u8>(address++) << 16;
				val |= read<u8>(address) << 24;
				return val;
			} else if (sizeof(T) == 2) {
				u16 val = read<u8>(address++);
				val |= read<u8>(address) << 8;
				return val;
			}

			switch (offset) {
			case 0x000 ... 0x056: // PPU
				return ppu.readIO(address);

			case 0x130: // Joypad
				return (u8)KEYINPUT;
			case 0x131:
				return (u8)(KEYINPUT >> 8);
			case 0x132:
				return (u8)KEYCNT;
			case 0x133:
				return (u8)(KEYCNT >> 8);

			case 0x200: // Interrupts
				return (u8)cpu.IE;
			case 0x201:
				return (u8)(cpu.IE >> 8);
			case 0x202:
				return (u8)cpu.IF;
			case 0x203:
				return (u8)(cpu.IF >> 8);
			case 0x208:
				return (u8)cpu.IME;
			}
			break;
		case toPage(0x5000000) ... toPage(0x6000000) - 1:
			std::memcpy(&val, &ppu.paletteRam[0] + (offset & 0x3FF), sizeof(T));
			return val;
		case toPage(0x7000000) ... toPage(0x8000000) - 1:
			std::memcpy(&val, &ppu.oam[0] + (offset & 0x3FF), sizeof(T));
			return val;
		}
	}

	return 0;
}
template u8 GameBoyAdvance::read<u8>(u32);
template u16 GameBoyAdvance::read<u16>(u32);
template u32 GameBoyAdvance::read<u32>(u32);

template <typename T>
void GameBoyAdvance::write(u32 address, T value) {
	int page = (address & 0x0FFFFFFF) >> 15;
	int offset = address & 0x7FFF;
	void *pointer = pageTableWrite[page];

	if (pointer != NULL) {
		std::memcpy((u8*)pointer + offset, &value, sizeof(T));
	} else {
		switch (page) {
		case toPage(0x4000000): // I/O
			if (sizeof(T) == 4) {
				write<u8>(address++, (u8)value);
				write<u8>(address++, (u8)(value >> 8));
				write<u8>(address++, (u8)(value >> 16));
				write<u8>(address, (u8)(value >> 24));
				return;
			} else if (sizeof(T) == 2) {
				write<u8>(address++, (u8)value);
				write<u8>(address, (u8)(value >> 8));
				return;
			}

			switch (offset) {
			case 0x000 ... 0x056: // PPU
				ppu.writeIO(address, value);
				break;

			case 0x131: // Joypad
				KEYCNT = (KEYCNT & 0xFF00) | value;
				break;
			case 0x132:
				KEYCNT = (KEYCNT & 0x00FF) | (value << 8);
				break;

			case 0x200: // Interrupts
				cpu.IE = (cpu.IE & 0x3F00) | value;
				break;
			case 0x201:
				cpu.IE = (cpu.IE & 0x00FF) | ((value & 0x3F) << 8);
				break;
			case 0x202:
				cpu.IF = (cpu.IF & ~value) & 0x3FFF;
				break;
			case 0x203:
				cpu.IF = (cpu.IF & ~(value << 8)) & 0x3FFF;
				break;
			case 0x208:
				cpu.IME = (bool)value;
				break;
			case 0x209:
			case 0x20A:
			case 0x20B:
				break;

			case 0x301: // HALTCNT
				cpu.halted = (bool)value;
				break;
			default:
				//printf("Unknown I/O register write:  %07X  %02X\n", address, value);
				break;
			}
			break;
		case toPage(0x5000000) ... toPage(0x6000000) - 1:
			if (sizeof(T) == 1) {
				std::memcpy(&ppu.paletteRam[0] + (offset & 0x3FE), &value, sizeof(T));
				std::memcpy(&ppu.paletteRam[0] + ((offset & 0x3FE) | 1), &value, sizeof(T));
			} else {
				std::memcpy(&ppu.paletteRam[0] + (offset & 0x3FF), &value, sizeof(T));
			}
			break;
		case toPage(0x7000000) ... toPage(0x8000000) - 1:
			if (sizeof(T) == 1) {
				std::memcpy(&ppu.oam[0] + (offset & 0x3FE), &value, sizeof(T));
				std::memcpy(&ppu.oam[0] + ((offset & 0x3FE) | 1), &value, sizeof(T));
			} else {
				std::memcpy(&ppu.oam[0] + (offset & 0x3FF), &value, sizeof(T));
			}
			break;
		}
	}
}
template void GameBoyAdvance::write<u8>(u32, u8);
template void GameBoyAdvance::write<u16>(u32, u16);
template void GameBoyAdvance::write<u32>(u32, u32);