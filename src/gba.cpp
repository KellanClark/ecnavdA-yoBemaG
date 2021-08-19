
#include "typedefs.hpp"
#include "gba.hpp"

namespace GBA {
	void reset() {
		running = false;
	}

	void run() {
		if (running) {
			//
		}
	}

	int loadRom(std::filesystem::path romFilePath_, std::filesystem::path biosFilePath_) {
		//

		return 0;
	}

	void save() {
		//
	}
};