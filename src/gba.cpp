
#include "gba.hpp"
#include "arm7tdmi.hpp"
#include "scheduler.hpp"
#include <cstddef>
#include <cstdio>

#define toPage(x) (x >> 15)

GameBoyAdvance::GameBoyAdvance() : cpu(*this), apu(*this), dma(*this), ppu(*this), timer(*this) {
	logFlash = false;

	//reset();
}

GameBoyAdvance::~GameBoyAdvance() {
	save();
}

void GameBoyAdvance::reset() {
	cpu.running = false;
	log.str("");

	flashState = READY;
	flashChipId = false;
	flashBank = 0;

	memset(ewram, 0, sizeof(ewram));
	memset(iwram, 0, sizeof(iwram));
	KEYINPUT = 0x3FF;
	KEYCNT = 0;
	POSTFLG = false;

	WAITCNT = 0;
	sramCycles = 4;
	ws0NonSequentialCycles = 4;
	ws0SequentialCycles = 2;
	ws1NonSequentialCycles = 4;
	ws1SequentialCycles = 4;
	ws2NonSequentialCycles = 4;
	ws2SequentialCycles = 8;

	systemEvents.reset();
	apu.reset();
	dma.reset();
	ppu.reset();
	timer.reset();
	cpu.reset();
}

bool GameBoyAdvance::searchRomForString(char *pattern, size_t patternSize) {
	for (size_t i = 0; i < romBuff.size(); i++) {
		size_t patternIndex = 0;
		while (romBuff[i + patternIndex] == pattern[patternIndex]) {
			if (patternIndex == (patternSize - 1))
				return true;

			++patternIndex;
		}
	}

	return false;
}

int GameBoyAdvance::loadBios(std::filesystem::path biosFilePath_) {
	if (biosFilePath_.empty()) {
		return -1;
	}

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

	return 0;
}

int GameBoyAdvance::loadRom(std::filesystem::path romFilePath_) {
	std::ifstream romFileStream{romFilePath_, std::ios::binary};
	if (!romFileStream.is_open()) {
		printf("Failed to open ROM file: %s\n", romFilePath_.c_str());
		return -1;
	}
	romFileStream.seekg(0, std::ios::end);
	romSize = romFileStream.tellg();
	romFileStream.seekg(0, std::ios::beg);
	romBuff.resize(romSize);
	romFileStream.read(reinterpret_cast<char *>(romBuff.data()), romSize);
	romFileStream.close();

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
	romBuff.resize(0x2000000);
	for (int i = romSize; i < 0x2000000; i += 2) {
		romBuff[i] = (i / 2) & 0xFF;
		romBuff[i + 1] = ((i / 2) >> 8) & 0xFF;
	}

	// Open save file
	saveFilePath = romFilePath_;
	saveFilePath.replace_extension(".sav");

	std::ifstream saveFileStream{saveFilePath, std::ios::binary};
	saveFileStream.seekg(0, std::ios::beg);

	// Get save type/size
	saveType = SRAM_32K;
	sram.resize(32 * 1024);
	char eeprom8KStr[] = "EEPROM_V";
	if (searchRomForString(eeprom8KStr, sizeof(eeprom8KStr) - 1)) {
		saveType = EEPROM_8K;
		sram.resize(8 * 1024);
	}
	char sram32KStr[] = "SRAM_V";
	if (searchRomForString(sram32KStr, sizeof(sram32KStr) - 1)) {
		saveType = SRAM_32K;
		sram.resize(32 * 1024);
	}
	char flash128KStr1[] = "FLASH_V";
	if (searchRomForString(flash128KStr1, sizeof(flash128KStr1) - 1)) {
		saveType = FLASH_128K;
		sram.resize(128 * 1024);
	}
	char flash128KStr2[] = "FLASH512_V";
	if (searchRomForString(flash128KStr2, sizeof(flash128KStr2) - 1)) {
		saveType = FLASH_128K;
		sram.resize(128 * 1024);
	}
	char flash128KStr3[] = "FLASH1M_V";
	if (searchRomForString(flash128KStr3, sizeof(flash128KStr3) - 1)) {
		saveType = FLASH_128K;
		sram.resize(128 * 1024);
	}

	saveFileStream.read(reinterpret_cast<char*>(sram.data()), sram.size());
	saveFileStream.close();

	return 0;
}

void GameBoyAdvance::save() {
	log << "Saving to " << saveFilePath << std::endl;
	std::ofstream saveFileStream{saveFilePath, std::ios::binary | std::ios::trunc};
	if (!saveFileStream) {
		log << "Failed to open/create save file\n";
		return;
	}
	saveFileStream.write(reinterpret_cast<const char*>(sram.data()), sram.size());
	saveFileStream.close();
}

u8 GameBoyAdvance::readDebug(u32 address) {
	int page = address >> 15;
	u32 offset;

	u8 val = 0;
	switch (page) {
	case toPage(0x0000000): // BIOS
		if (address <= 0x3FFF) {
			val = biosBuff[address];
		}
		break;
	case toPage(0x2000000) ... toPage(0x3000000) - 1: // EWRAM
		val = ewram[address & 0x3FFFF];
		break;
	case toPage(0x3000000) ... toPage(0x4000000) - 1: // IWRAM
		val = iwram[address & 0x7FFF];
		break;
	case toPage(0x4000000): // I/O
		val = readIO(address);
		break;
	case toPage(0x5000000) ... toPage(0x6000000) - 1: // Palette RAM
		val = ppu.paletteRam[address & 0x3FF];
		break;
	case toPage(0x6000000) ... toPage(0x7000000) - 1: // VRAM
		offset = address & 0x1FFFF;
		if (offset > 0x17FFF)
			offset -= 0x8000;
		val = ppu.paletteRam[offset];
		break;
	case toPage(0x7000000) ... toPage(0x8000000) - 1: // OAM
		val = ppu.oam[address & 0x3FF];
		break;
	case toPage(0x8000000) ... toPage(0xA000000) - 1: // ROM
	case toPage(0xA000000) ... toPage(0xC000000) - 1:
	case toPage(0xC000000) ... toPage(0xE000000) - 1:
		val = romBuff[address & 0x1FFFFFF];
		break;
	case toPage(0xE000000) ... toPage(0x10000000) - 1:
		if (saveType == SRAM_32K) {
			val = sram[address & 0x7FFF];
		} else if (saveType == FLASH_128K) {
			offset = address & 0xFFFF;
			if (flashChipId && (offset == 0)) { [[unlikely]] // Read chip ID instead of data
				val = 0x62;
			} else if (flashChipId && (offset == 1)) { [[unlikely]]
				val = 0x13;
			} else {
				val = sram[flashBank | (address & 0xFFFF)];
			}
		}
		break;
	}

	return val;
}

template <typename T>
T GameBoyAdvance::openBus(u32 address) {
	return (T)(((address <= 0x3FFF) ? biosOpenBusValue : openBusValue) >> ((sizeof(T) == 1) ? ((address & 3) * 8) : 0));
}
template u8 GameBoyAdvance::openBus<u8>(u32);
template u16 GameBoyAdvance::openBus<u16>(u32);
template u32 GameBoyAdvance::openBus<u32>(u32);

template <typename T, bool rotate>
u32 GameBoyAdvance::read(u32 address, bool sequential) {
	int page = address >> 15;
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	u32 offset;

	sequential = sequential && !forceNonSequential && (address & 0x1FFFF);
	forceNonSequential = false;

	u32 val = openBus<T>(address);
	switch (page) {
	case toPage(0x0000000): // BIOS
		systemEvents.tickScheduler(1);

		if ((address <= 0x3FFF) && (cpu.reg.R[15] <= 0x3FFF)) {
			if (cpu.hleBios) {
				// Intercept jumps to BIOS
				if (!sequential && (address == (cpu.reg.R[15] - (cpu.reg.thumbMode ? 4 : 8)))) {
					cpu.bios.processJump = true;
				}
			} else {
				std::memcpy(&val, (u8*)biosBuff.data() + alignedAddress, sizeof(T));
			}
		}
		break;
	case toPage(0x2000000) ... toPage(0x3000000) - 1: // EWRAM
		if constexpr (sizeof(T) == 4) {
			systemEvents.tickScheduler(6);
		} else {
			systemEvents.tickScheduler(3);
		}

		std::memcpy(&val, &ewram[0] + (alignedAddress & 0x3FFFF), sizeof(T));
		break;
	case toPage(0x3000000) ... toPage(0x4000000) - 1: // IWRAM
		systemEvents.tickScheduler(1);

		std::memcpy(&val, &iwram[0] + (alignedAddress & 0x7FFF), sizeof(T));
		break;
	case toPage(0x4000000): // I/O
		systemEvents.tickScheduler(1);

		// Split everything into u8
		if (sizeof(T) == 4) {
			u32 val = readIO(alignedAddress | 0);
			val |= readIO(alignedAddress | 1) << 8;
			val |= readIO(alignedAddress | 2) << 16;
			val |= readIO(alignedAddress | 3) << 24;
			return val;
		} else if (sizeof(T) == 2) {
			u16 val = readIO(alignedAddress | 0);
			val |= readIO(alignedAddress | 1) << 8;
			return val;
		} else if (sizeof(T) == 1) {
			return readIO(address);
		}
		break;
	case toPage(0x5000000) ... toPage(0x6000000) - 1: // Palette RAM
		if constexpr (sizeof(T) == 4) {
			systemEvents.tickScheduler(2);
		} else {
			systemEvents.tickScheduler(1);
		}

		std::memcpy(&val, &ppu.paletteRam[0] + (alignedAddress & 0x3FF), sizeof(T));
		break;
	case toPage(0x6000000) ... toPage(0x7000000) - 1: // VRAM
		if constexpr (sizeof(T) == 4) {
			systemEvents.tickScheduler(2);
		} else {
			systemEvents.tickScheduler(1);
		}

		offset = alignedAddress & 0x1FFFF;
		if (offset > 0x17FFF)
			offset -= 0x8000;
		std::memcpy(&val, &ppu.vram[0] + offset, sizeof(T));
		break;
	case toPage(0x7000000) ... toPage(0x8000000) - 1: // OAM
		systemEvents.tickScheduler(1);

		std::memcpy(&val, &ppu.oam[0] + (alignedAddress & 0x3FF), sizeof(T));
		break;
	case toPage(0x8000000) ... toPage(0xA000000) - 1: // ROM (Waitstate 0)
		systemEvents.tickScheduler((sequential ? ws0SequentialCycles : ws0NonSequentialCycles) + ((sizeof(T) == 4) ? (ws0SequentialCycles + 2) : 1));

		std::memcpy(&val, (u8*)romBuff.data() + (alignedAddress & 0x1FFFFFF), sizeof(T));
		break;
	case toPage(0xA000000) ... toPage(0xC000000) - 1: // ROM (Waitstate 1)
		systemEvents.tickScheduler((sequential ? ws1SequentialCycles : ws1NonSequentialCycles) + ((sizeof(T) == 4) ? (ws1SequentialCycles + 2) : 1));

		std::memcpy(&val, (u8*)romBuff.data() + (alignedAddress & 0x1FFFFFF), sizeof(T));
		break;
	case toPage(0xC000000) ... toPage(0xE000000) - 1: // ROM (Waitstate 2)
		systemEvents.tickScheduler((sequential ? ws2SequentialCycles : ws2NonSequentialCycles) + ((sizeof(T) == 4) ? (ws2SequentialCycles + 2) : 1));

		std::memcpy(&val, (u8*)romBuff.data() + (alignedAddress & 0x1FFFFFF), sizeof(T));
		break;
	case toPage(0xE000000) ... toPage(0x10000000) - 1:
		systemEvents.tickScheduler(1);

		if (saveType == SRAM_32K) {
			val = sram[address & 0x7FFF];

			if constexpr (sizeof(T) == 2) {
				val *= 0x0101;
			} else if constexpr (sizeof(T) == 4) {
				val *= 0x01010101;
			}
		} else if (saveType == FLASH_128K) {
			offset = address & 0xFFFF;
			if (flashChipId && (offset == 0)) { [[unlikely]] // Read chip ID instead of data
				val = 0x62;
			} else if (flashChipId && (offset == 1)) { [[unlikely]]
				val = 0x13;
			} else {
				val = sram[flashBank | (address & 0xFFFF)];
			}
		}
		break;
	default:
		systemEvents.tickScheduler(1);
		break;
	}

	u32 newOpenBus = 0;
	if constexpr (sizeof(T) == 2) {
		switch (page) {
		case toPage(0x2000000) ... toPage(0x3000000) - 1: // EWRAM
		case toPage(0x5000000) ... toPage(0x6000000) - 1: // Palette RAM
		case toPage(0x6000000) ... toPage(0x7000000) - 1: // VRAM
		case toPage(0x8000000) ... toPage(0xE000000) - 1: // Cartridge ROM
			newOpenBus = (val << 16) | val;
			break;

		case toPage(0x0000000) ... toPage(0x2000000) - 1: // BIOS
			newOpenBus = (val << 16) | (biosOpenBusValue >> 16);
			break;
		case toPage(0x7000000) ... toPage(0x8000000) - 1: // OAM
			newOpenBus = (val << 16) | (openBusValue >> 16);
			break;

		case toPage(0x3000000) ... toPage(0x4000000) - 1: // IWRAM
			if (address & 2) {
				newOpenBus = (val << 16) | (openBusValue & 0x00FF);
			} else {
				newOpenBus = (openBusValue & 0xFF00) | val;
			}
			break;
		}
	} else if constexpr (sizeof(T) == 4) {
		newOpenBus = val;
	}
	if (cpu.reg.R[15] < 0x2000000) {
		biosOpenBusValue = newOpenBus;
	} else {
		openBusValue = newOpenBus;
	}

	if constexpr (rotate) {
		// Rotate misaligned loads
		if ((sizeof(T) == 2) && (address & 1)) [[unlikely]]
			val = (val >> 8) | (val << 24);
		if ((sizeof(T) == 4) && (address & 3)) [[unlikely]]
			val = (val << ((4 - (address & 3)) * 8)) | (val >> ((address & 3) * 8));
	}

	return val;
}
template u32 GameBoyAdvance::read<u8>(u32, bool);
template u32 GameBoyAdvance::read<u16>(u32, bool);
template u32 GameBoyAdvance::read<u32>(u32, bool);
template u32 GameBoyAdvance::read<u32, false>(u32, bool);

u8 GameBoyAdvance::readIO(u32 address) {
	int offset = address & 0x7FFF;
	switch (offset) {
	case 0x000 ... 0x055: // PPU
		return ppu.readIO(address);

	case 0x060 ... 0x0A7: // APU
		return apu.readIO(address);

	case 0x0B0 ... 0x0DF: // DMA
		return dma.readIO(address);

	case 0x100 ... 0x10F: // Timer
		return timer.readIO(address);

	case 0x130: // Joypad
		return (u8)KEYINPUT;
	case 0x131:
		return (u8)(KEYINPUT >> 8);
	case 0x132:
		return (u8)KEYCNT;
	case 0x133:
		return (u8)(KEYCNT >> 8);

	case 0x200: // Misc.
		return (u8)cpu.IE;
	case 0x201:
		return (u8)(cpu.IE >> 8);
	case 0x202:
		return (u8)cpu.IF;
	case 0x203:
		return (u8)(cpu.IF >> 8);
	case 0x204:
		return (u8)WAITCNT;
	case 0x205:
		return (u8)(WAITCNT >> 8);
	case 0x206:
	case 0x207:
		return 0;
	case 0x208:
		return (u8)cpu.IME;
	case 0x209:
	case 0x20A:
	case 0x20B:
		return 0;
	case 0x300:
		return (u8)POSTFLG;
	case 0x301:
	case 0x302:
	case 0x303:
		return 0;

	default:
		return openBus<u8>(address);
	}
}

void GameBoyAdvance::writeDebug(u32 address, u8 value, bool unrestricted) {
	int page = address >> 15;
	int offset;

	switch (page) {
	case toPage(0x0000000): // BIOS
		if (unrestricted && (address < 0x3FFF))
			biosBuff[address] = value;
	case toPage(0x2000000) ... toPage(0x3000000) - 1: // EWRAM
		ewram[address & 0x3FFFF] = value;
		break;
	case toPage(0x3000000) ... toPage(0x4000000) - 1: // IWRAM
		iwram[address & 0x7FFF] = value;
		break;
	case toPage(0x4000000): // I/O
		writeIO(address, value);
		break;
	case toPage(0x5000000) ... toPage(0x6000000) - 1: // Palette RAM
		ppu.paletteRam[address & 0x3FF] = value;
		break;
	case toPage(0x6000000) ... toPage(0x7000000) - 1: // VRAM
		offset = address & 0x1FFFF;
		if (offset > 0x17FFF)
			offset -= 0x8000;
		ppu.vram[offset] = value;
		break;
	case toPage(0x7000000) ... toPage(0x8000000) - 1: // OAM
		ppu.oam[address & 0x3FF] = value;
		break;
	case toPage(0x8000000) ... toPage(0xE000000) - 1: // ROM
		offset = address & 0x1000000;
		if (unrestricted && (offset < romSize)) {
			romBuff[offset] = value;
		}
		break;
	case toPage(0xE000000) ... toPage(0x10000000) - 1:
		if (saveType == SRAM_32K) {
			sram[address & 0x7FFF] = value;
		} else if (saveType == FLASH_128K) {
			value = (u8)value;
			offset = address & 0xFFFF;

			if (unrestricted) {
				sram[offset] = value;
			} else if ((offset == 0x0000) && (flashState & BANK)) {
				flashBank = (value & 1) << 16;
				flashState = READY;

				if (logFlash)
					log << "Flash command 0xB0: Chose bank " << (value & 1) << "\n";
			} else if (flashState & WRITE) {
				sram[flashBank | offset] = value;
				flashState = READY;

				if (logFlash)
					log << fmt::format("Flash command 0xA0: Wrote 0x{:0>2X} to 0x{:0>4X}\n", value, (flashBank | offset));
			} else if (offset == 0x2AAA) {
				if ((flashState & CMD_1) && (value == 0x55)) {
					flashState &= ~CMD_1;
					flashState |= CMD_2;
				}
			} else if (offset == 0x5555) {
				if ((flashState & READY) && (value == 0xAA)) {
					flashState &= ~READY;
					flashState |= CMD_1;
				} else if (flashState & CMD_2) {
					if ((value == 0x10) && (flashState & ERASE)) { // Erase entire chip
						memset(sram.data(), 0xFF, sram.size());
						flashState = READY;

						if (logFlash)
							log << "Flash command 0x80-0x10: Erase entire chip\n";
					} else if ((value == 0x80) && !(flashState & ~0x7)) { // Prepare for erase command
						flashState = READY | ERASE;

						if (logFlash)
							log << "Flash command 0x80: Prepare for erase command\n";
					} else if ((value == 0x90) && !(flashState & ~0x7)) { // Enter Chip ID mode
						flashChipId = true;
						flashState = READY;

						if (logFlash)
							log << "Flash command 0x90: Enter Chip ID mode\n";
					} else if ((value == 0xA0) && !(flashState & ~0x7)) { // Prepare for write
						flashState = WRITE;

						if (logFlash)
							log << "Flash command 0xA0: Prepare for write\n";
					} else if ((value == 0xB0) && !(flashState & ~0x7) && (saveType == FLASH_128K)) { // Select memory bank
						flashState = BANK;

						if (logFlash)
							log << "Flash command 0xB0: Select memory bank\n";
					} else if ((value == 0xF0) && !(flashState & ~0x7)) { // Exit Chip ID mode
						flashChipId = false;
						flashState = READY;

						if (logFlash)
							log << "Flash command 0xF0: Exit Chip ID mode\n";
					}
				}
			} else if ((offset & 0xFFF) == 0) { // Erase 4KB sector
				if ((value == 0x30) && (flashState & (CMD_2 | ERASE))) {
					memset(&sram[flashBank | (offset & 0xF000)], 0xFF, 0x1000);
					flashState = READY;

					if (logFlash)
						log << fmt::format("Flash command 0x80-0x30: Erase 4KB sector 0x{0:X}000-0x{0:X}FFF\n", (flashBank | (offset & 0xF000)) >> 12);
				}
			}
		}
		break;
	}
}

template <typename T>
void GameBoyAdvance::write(u32 address, T value, bool sequential) {
	int page = address >> 15;
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	int offset;

	sequential = sequential && !forceNonSequential && (address & 0x1FFFF);
	forceNonSequential = false;

	switch (page) {
	case toPage(0x2000000) ... toPage(0x3000000) - 1: // EWRAM
		if constexpr (sizeof(T) == 4) {
			systemEvents.tickScheduler(6);
		} else {
			systemEvents.tickScheduler(3);
		}

		std::memcpy(&ewram[0] + (alignedAddress & 0x3FFFF), &value, sizeof(T));
		break;
	case toPage(0x3000000) ... toPage(0x4000000) - 1: // IWRAM
		systemEvents.tickScheduler(1);

		std::memcpy(&iwram[0] + (alignedAddress & 0x7FFF), &value, sizeof(T));
		break;
	case toPage(0x4000000): // I/O
		systemEvents.tickScheduler(1);

		if constexpr (sizeof(T) == 4) {
			writeIO((address & ~3) | 0, (u8)value);
			writeIO((address & ~3) | 1, (u8)(value >> 8));
			writeIO((address & ~3) | 2, (u8)(value >> 16));
			writeIO((address & ~3) | 3, (u8)(value >> 24));
			return;
		} else if constexpr (sizeof(T) == 2) {
			writeIO((address & ~1) | 0, (u8)value);
			writeIO((address & ~1) | 1, (u8)(value >> 8));
			return;
		} else if (sizeof(T) == 1) {
			writeIO(address, value);
			return;
		}
		break;
	case toPage(0x5000000) ... toPage(0x6000000) - 1: // Palette RAM
		if constexpr (sizeof(T) == 4) {
			systemEvents.tickScheduler(2);
		} else {
			systemEvents.tickScheduler(1);
		}

		if constexpr (sizeof(T) == 1) {
			ppu.paletteRam[alignedAddress & 0x3FE] = value;
			ppu.paletteRam[(alignedAddress & 0x3FE) | 1] = value;
		} else {
			std::memcpy(&ppu.paletteRam[0] + (alignedAddress & 0x3FF), &value, sizeof(T));
		}
		break;
	case toPage(0x6000000) ... toPage(0x7000000) - 1: // VRAM
		if constexpr (sizeof(T) == 4) {
			systemEvents.tickScheduler(2);
		} else {
			systemEvents.tickScheduler(1);
		}

		offset = alignedAddress & 0x1FFFF;
		if (offset > 0x17FFF)
			offset -= 0x8000;
		if constexpr (sizeof(T) == 1) {
			if (offset <= ((ppu.bgMode > 2) ? 0x13FFF : 0xFFFF)) {
				ppu.vram[offset & ~1] = value;
				ppu.vram[(offset & ~1) | 1] = value;
			}
		} else {
			std::memcpy(&ppu.vram[0] + offset, &value, sizeof(T));
		}
		break;
	case toPage(0x7000000) ... toPage(0x8000000) - 1: // OAM
		systemEvents.tickScheduler(1);

		if constexpr (sizeof(T) != 1) {
			std::memcpy(&ppu.oam[0] + (alignedAddress & 0x3FF), &value, sizeof(T));
		}
		break;
	case toPage(0x8000000) ... toPage(0xA000000) - 1: // ROM (Waitstate 0)
		systemEvents.tickScheduler((sequential ? ws0SequentialCycles : ws0NonSequentialCycles) + ((sizeof(T) == 4) ? (ws0SequentialCycles + 2) : 1));
		break;
	case toPage(0xA000000) ... toPage(0xC000000) - 1: // ROM (Waitstate 1)
		systemEvents.tickScheduler((sequential ? ws1SequentialCycles : ws1NonSequentialCycles) + ((sizeof(T) == 4) ? (ws1SequentialCycles + 2) : 1));
		break;
	case toPage(0xC000000) ... toPage(0xE000000) - 1: // ROM (Waitstate 2)
		systemEvents.tickScheduler((sequential ? ws2SequentialCycles : ws2NonSequentialCycles) + ((sizeof(T) == 4) ? (ws2SequentialCycles + 2) : 1));
		break;
	case toPage(0xE000000) ... toPage(0x10000000) - 1: // SRAM/Flash
		systemEvents.tickScheduler(1);

		if (saveType == SRAM_32K) {
			sram[address & 0x7FFF] = (u8)value;
		} else if (saveType == FLASH_128K) {
			value = (u8)value;
			offset = address & 0xFFFF;

			if ((offset == 0x0000) && (flashState & BANK)) {
				flashBank = (value & 1) << 16;
				flashState = READY;

				if (logFlash)
					log << "Flash command 0xB0: Chose bank " << (value & 1) << "\n";
			} else if (flashState & WRITE) {
				sram[flashBank | offset] = value;
				flashState = READY;

				if (logFlash)
					log << fmt::format("Flash command 0xA0: Wrote 0x{:0>2X} to 0x{:0>4X}\n", value, (flashBank | offset));
			} else if (offset == 0x2AAA) {
				if ((flashState & CMD_1) && (value == 0x55)) {
					flashState &= ~CMD_1;
					flashState |= CMD_2;
				}
			} else if (offset == 0x5555) {
				if ((flashState & READY) && (value == 0xAA)) {
					flashState &= ~READY;
					flashState |= CMD_1;
				} else if (flashState & CMD_2) {
					if ((value == 0x10) && (flashState & ERASE)) { // Erase entire chip
						memset(sram.data(), 0xFF, sram.size());
						flashState = READY;

						if (logFlash)
							log << "Flash command 0x80-0x10: Erase entire chip\n";
					} else if ((value == 0x80) && !(flashState & ~0x7)) { // Prepare for erase command
						flashState = READY | ERASE;

						if (logFlash)
							log << "Flash command 0x80: Prepare for erase command\n";
					} else if ((value == 0x90) && !(flashState & ~0x7)) { // Enter Chip ID mode
						flashChipId = true;
						flashState = READY;

						if (logFlash)
							log << "Flash command 0x90: Enter Chip ID mode\n";
					} else if ((value == 0xA0) && !(flashState & ~0x7)) { // Prepare for write
						flashState = WRITE;

						if (logFlash)
							log << "Flash command 0xA0: Prepare for write\n";
					} else if ((value == 0xB0) && !(flashState & ~0x7) && (saveType == FLASH_128K)) { // Select memory bank
						flashState = BANK;

						if (logFlash)
							log << "Flash command 0xB0: Select memory bank\n";
					} else if ((value == 0xF0) && !(flashState & ~0x7)) { // Exit Chip ID mode
						flashChipId = false;
						flashState = READY;

						if (logFlash)
							log << "Flash command 0xF0: Exit Chip ID mode\n";
					}
				}
			} else if ((offset & 0xFFF) == 0) { // Erase 4KB sector
				if ((value == 0x30) && (flashState & (CMD_2 | ERASE))) {
					memset(&sram[flashBank | (offset & 0xF000)], 0xFF, 0x1000);
					flashState = READY;

					if (logFlash)
						log << fmt::format("Flash command 0x80-0x30: Erase 4KB sector 0x{0:X}000-0x{0:X}FFF\n", (flashBank | (offset & 0xF000)) >> 12);
				}
			}
		}
		break;
	default:
		systemEvents.tickScheduler(1);
		break;
	}
}
template void GameBoyAdvance::write<u8>(u32, u8, bool);
template void GameBoyAdvance::write<u16>(u32, u16, bool);
template void GameBoyAdvance::write<u32>(u32, u32, bool);

static const int waitCycleTable[4] = {4, 3, 2, 8};

void GameBoyAdvance::writeIO(u32 address, u8 value) {
	int offset = address & 0x7FFF;
	switch (offset) {
	case 0x000 ... 0x055: // PPU
		ppu.writeIO(address, value);
		break;

	case 0x060 ... 0x0A7: // APU
		apu.writeIO(address, value);
		break;

	case 0x0B0 ... 0x0DF: // DMA
		dma.writeIO(address, value);
		break;

	case 0x100 ... 0x10F: // Timer
		timer.writeIO(address, value);
		break;

	case 0x132: // Joypad
		KEYCNT = (KEYCNT & 0xFF00) | value; // TODO
		break;
	case 0x133:
		KEYCNT = (KEYCNT & 0x00FF) | ((value & 0xC3) << 8);
		break;

	case 0x200: // Misc.
		cpu.IE = (cpu.IE & 0x3F00) | value;
		cpu.testInterrupt();
		break;
	case 0x201:
		cpu.IE = (cpu.IE & 0x00FF) | ((value & 0x3F) << 8);
		cpu.testInterrupt();
		break;
	case 0x202:
		cpu.IF = (cpu.IF & ~value) & 0x3FFF;
		cpu.testInterrupt();
		break;
	case 0x203:
		cpu.IF = (cpu.IF & ~(value << 8)) & 0x3FFF;
		cpu.testInterrupt();
		break;
	case 0x204:
		WAITCNT = (WAITCNT & 0xFF00) | value;

		sramCycles = waitCycleTable[sramWaitControl];
		ws0NonSequentialCycles = waitCycleTable[ws0NonSequentialControl];
		ws0SequentialCycles = ws0SequentialControl ? 1 : 2;
		ws1NonSequentialCycles = waitCycleTable[ws1NonSequentialControl];
		ws1SequentialCycles = ws1SequentialControl ? 1 : 4;
		break;
	case 0x205:
		WAITCNT = (WAITCNT & 0x00FF) | ((value & 0x5F) << 8);

		ws2NonSequentialCycles = waitCycleTable[ws2NonSequentialControl];
		ws2SequentialCycles = ws2SequentialControl ? 1 : 8;
		break;
	case 0x208:
		cpu.IME = (bool)(value & 1);
		cpu.testInterrupt();
		break;
	case 0x300: // POSTFLG
		if ((cpu.reg.R[15] <= 0x3FFF) && (!POSTFLG))
			POSTFLG = (bool)(value & 1);
		break;
	case 0x301: // HALTCNT
		if (cpu.reg.R[15] <= 0x3FFF) {
			if (value & 0x80) { // TODO: Are these mutally exclusive?
				cpu.stopped = true;
			} else {
				cpu.halted = true;
			}
			cpu.testInterrupt();
		}
		break;

	default:
		//printf("Unknown I/O register write:  %07X  %02X\n", address, value);
		break;
	}
}

void GameBoyAdvance::internalCycle(int cycles) {
	forceNonSequential = true;
	systemEvents.tickScheduler(cycles);
}
