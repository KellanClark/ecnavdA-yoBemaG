
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
	cpu.cycle();
	while (running) {
		//
	}
}

int GameBoyAdvance::loadRom(std::filesystem::path romFilePath_, std::filesystem::path biosFilePath_) {
	//

	return 0;
}

void GameBoyAdvance::save() {
	//
}