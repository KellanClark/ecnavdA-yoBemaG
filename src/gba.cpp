
#include "gba.hpp"
#include "scheduler.hpp"
#include <cstddef>
#include <cstdio>

#define toPage(x) (x >> 15)

GameBoyAdvance::GameBoyAdvance() : cpu(*this), apu(*this), dma(*this), ppu(*this), timer(*this) {
	logFlash = false;

	// Fill page tables
	for (int i = toPage(0x2000000); i < toPage(0x3000000); i += (256 / 32)) { // EWRAM
		for (int j = i; j < (i + (256 / 32)); j++) {
			pageTableRead[j] = pageTableWrite[j] = pageTableWriteByte[j] = &ewram[(j - i) << 15];
		}
	}
	for (int i = toPage(0x3000000); i < toPage(0x4000000); i++) { // IWRAM
		pageTableRead[i] = pageTableWrite[i] = pageTableWriteByte[i] = &iwram[0];
	}
	for (int i = toPage(0x6000000); i < toPage(0x7000000); i += 4) { // VRAM
		pageTableRead[i] = pageTableWrite[i] = &ppu.vram[0];
		pageTableRead[i + 1] = pageTableWrite[i + 1] = &ppu.vram[0x08000];
		pageTableRead[i + 2] = pageTableWrite[i + 2] = &ppu.vram[0x10000];
		pageTableRead[i + 3] = pageTableWrite[i + 3] = &ppu.vram[0x10000];
	}

	reset();
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

	systemEvents.reset();
	cpu.reset();
	apu.reset();
	dma.reset();
	ppu.reset();
	timer.reset();
}

static bool searchForString(char *array, size_t arraySize, char *pattern, size_t patternSize) {
	size_t i = -1;
	while (i < arraySize) {
		size_t patternIndex = 0;
		++i;
		while (array[i] == pattern[patternIndex]) {
			if (patternIndex == (patternSize - 1))
				return true;

			++patternIndex;
			++i;
		}
	}

	return false;
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
		romBuff[i] = (i / 2) & 0xFF;
		romBuff[i + 1] = ((i / 2) >> 8) & 0xFF;
	}

	// Fill page table for rom buffer
	for (int pageIndex = 0; pageIndex < toPage(0x2000000); pageIndex++) {
		u8 *ptr = &romBuff[pageIndex << 15];
		pageTableRead[pageIndex | toPage(0x8000000)] = ptr; // 0x0800'0000 - 0x09FF'FFFF
		pageTableRead[pageIndex | toPage(0xA000000)] = ptr; // 0x0A00'0000 - 0x0BFF'FFFF
		pageTableRead[pageIndex | toPage(0xC000000)] = ptr; // 0x0C00'0000 - 0x0DFF'FFFF
	}

	// Open save file
	saveFilePath = romFilePath_;
	saveFilePath.replace_extension(".sav");

	std::ifstream saveFileStream{saveFilePath, std::ios::binary};
	saveFileStream.seekg(0, std::ios::beg);

	// Get save type/size
	saveType = SRAM_32K;
	sram.resize(32 * 1024);
	//saveType = FLASH_128K;
	//sram.resize(128 * 1024);
	char eeprom8KStr[] = "EEPROM_V";
	if (searchForString((char *)romBuff.data(), romBuff.size(), eeprom8KStr, sizeof(eeprom8KStr) - 1)) {
		saveType = EEPROM_8K;
		sram.resize(8 * 1024);
	}
	char sram32KStr[] = "SRAM_V";
	if (searchForString((char *)romBuff.data(), romBuff.size(), sram32KStr, sizeof(sram32KStr) - 1)) {
		saveType = SRAM_32K;
		sram.resize(32 * 1024);
	}
	char flash128KStr1[] = "FLASH_V";
	if (searchForString((char *)romBuff.data(), romBuff.size(), flash128KStr1, sizeof(flash128KStr1) - 1)) {
		saveType = FLASH_128K;
		sram.resize(128 * 1024);
	}
	char flash128KStr2[] = "FLASH512_V";
	if (searchForString((char *)romBuff.data(), romBuff.size(), flash128KStr2, sizeof(flash128KStr2) - 1)) {
		saveType = FLASH_128K;
		sram.resize(128 * 1024);
	}
	char flash128KStr3[] = "FLASH1M_V";
	if (searchForString((char *)romBuff.data(), romBuff.size(), flash128KStr3, sizeof(flash128KStr3) - 1)) {
		saveType = FLASH_128K;
		sram.resize(128 * 1024);
	}

	saveFileStream.read(reinterpret_cast<char*>(sram.data()), sram.size());
	saveFileStream.close();

	cpu.running = true;
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

template <typename T>
T GameBoyAdvance::openBus(u32 address) {
	return (T)(((address <= 0x3FFF) ? biosOpenBusValue : openBusValue) >> ((sizeof(T) == 1) ? ((address & 3) * 8) : 0));
}
template u8 GameBoyAdvance::openBus<u8>(u32);
template u16 GameBoyAdvance::openBus<u16>(u32);
template u32 GameBoyAdvance::openBus<u32>(u32);

template <typename T>
u32 GameBoyAdvance::read(u32 address) {
	int page = (address & 0x0FFFFFFF) >> 15;
	int offset = address & 0x7FFF & ~(sizeof(T) - 1);
	void *pointer = pageTableRead[page];

	u32 val = openBus<T>(address);
	if (address <= 0x0FFFFFFF) { [[likely]]
		if (pointer != NULL) {
			std::memcpy(&val, (u8*)pointer + offset, sizeof(T));
		} else {
			switch (page) {
			case toPage(0x0000000): // BIOS
				if ((address <= 0x3FFF) && (cpu.reg.R[15] < 0x2000000)) {
					std::memcpy(&val, (u8*)biosBuff.data() + offset, sizeof(T));
				}
				break;
			case toPage(0x4000000): // I/O
				// Split everything into u8
				if (sizeof(T) == 4) {
					u32 val = read<u8>((address & ~3) | 0);
					val |= read<u8>((address & ~3) | 1) << 8;
					val |= read<u8>((address & ~3) | 2) << 16;
					val |= read<u8>((address & ~3) | 3) << 24;
					return val;
				} else if (sizeof(T) == 2) {
					u16 val = read<u8>((address & ~1) | 0);
					val |= read<u8>((address & ~1) | 1) << 8;
					return val;
				}

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
				
				default:
					return openBus<u8>(address);
				}
				break;
			case toPage(0x5000000) ... toPage(0x6000000) - 1: // Palette RAM
				std::memcpy(&val, &ppu.paletteRam[0] + (offset & 0x3FF), sizeof(T));
				break;
			case toPage(0x7000000) ... toPage(0x8000000) - 1: // OAM
				std::memcpy(&val, &ppu.oam[0] + (offset & 0x3FF), sizeof(T));
				break;
			case toPage(0xE000000) ... toPage(0x10000000) - 1:
				if (saveType == SRAM_32K) {
					val = sram[address & 0x7FFF];

					if (sizeof(T) == 2) {
						val *= 0x0101;
					} else if (sizeof(T) == 4) {
						val *= 0x01010101;
					}
				} else if (saveType == FLASH_128K) {
					if (flashChipId && ((address & 0xFFFF) < 2)) { // Read chip ID instead of data
						if ((address & 0xFFFF) == 2) {
							return 0x62;
						} else {
							return 0x13;
						}
					}
					val = sram[flashBank | (address & 0xFFFF)];

					if (sizeof(T) == 2) {
						val *= 0x0101;
					} else if (sizeof(T) == 4) {
						val *= 0x01010101;
					}
				}
				break;
			}
		}
	}

	u32 newOpenBus = 0;
	if (sizeof(T) == 2) {
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
	} else if (sizeof(T) == 4) {
		newOpenBus = val;
	}
	if (cpu.reg.R[15] < 0x2000000) {
		biosOpenBusValue = newOpenBus;
	} else {
		openBusValue = newOpenBus;
	}

	// Rotate misaligned loads
	if ((sizeof(T) == 2) && (address & 1)) [[unlikely]]
		val = (val >> 8) | (val << 24);
	if ((sizeof(T) == 4) && (address & 3)) [[unlikely]]
		val = (val << ((4 - (address & 3)) * 8)) | (val >> ((address & 3) * 8));

	return val;
}
template u32 GameBoyAdvance::read<u8>(u32);
template u32 GameBoyAdvance::read<u16>(u32);
template u32 GameBoyAdvance::read<u32>(u32);

template <typename T>
void GameBoyAdvance::write(u32 address, T value) {
	int page = (address & 0x0FFFFFFF) >> 15;
	int offset = address & 0x7FFF & ~(sizeof(T) - 1);
	void *pointer = (sizeof(T) == 1) ? pageTableWriteByte[page] : pageTableWrite[page];

	if (address <= 0x0FFFFFFF) { [[likely]]
		if (pointer != NULL) {
			std::memcpy((u8*)pointer + offset, &value, sizeof(T));
		} else {
			switch (page) {
			case toPage(0x4000000): // I/O
				if (sizeof(T) == 4) {
					write<u8>((address & ~3) | 0, (u8)value);
					write<u8>((address & ~3) | 1, (u8)(value >> 8));
					write<u8>((address & ~3) | 2, (u8)(value >> 16));
					write<u8>((address & ~3) | 3, (u8)(value >> 24));
					return;
				} else if (sizeof(T) == 2) {
					write<u8>((address & ~1) | 0, (u8)value);
					write<u8>((address & ~1) | 1, (u8)(value >> 8));
					return;
				}

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
					KEYCNT = (KEYCNT & 0xFF00) | value;
					break;
				case 0x133:
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
			case toPage(0x5000000) ... toPage(0x6000000) - 1: // Palette RAM
				if (sizeof(T) == 1) {
					ppu.paletteRam[offset & 0x3FE] = value;
					ppu.paletteRam[(offset & 0x3FE) | 1] = value;
				} else {
					std::memcpy(&ppu.paletteRam[0] + (offset & 0x3FF), &value, sizeof(T));
				}
				break;
			case toPage(0x6000000) ... toPage(0x7000000) - 1: // VRAM (byte only)
				if ((address & 0x1FFFF) < 0x10000) {
					ppu.vram[address & 0xFFFF] = value;
					ppu.vram[(address & 0xFFFF) | 1] = value;
				} else {
					address &= 0x7FFF;
					if ((ppu.bgMode > 2) && (address < 0x4000)) {
						ppu.vram[(address & 0x7FFF) | 0x10000] = value;
						ppu.vram[(address & 0x7FFF) | 0x10001] = value;
					}
				}
				break;
			case toPage(0x7000000) ... toPage(0x8000000) - 1: // OAM
				if (sizeof(T) != 1) {
					std::memcpy(&ppu.oam[0] + (offset & 0x3FF), &value, sizeof(T));
				}
				break;
			case toPage(0xE000000) ... toPage(0x10000000) - 1:
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
			}
		}
	}
}
template void GameBoyAdvance::write<u8>(u32, u8);
template void GameBoyAdvance::write<u16>(u32, u16);
template void GameBoyAdvance::write<u32>(u32, u32);