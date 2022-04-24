# ecnavdA-yoBemaG
A WIP Game Boy Advance emulator.

## Major missing features
* Proper transparency and alpha blending
* Prefetch buffer
* EEPROM saves
* All extra cartridge hardware
* Serial
* [HLE BIOS] Accurate timings
* [HLE BIOS] Correct boot state
* [HLE BIOS] Non-IRQ exception handling
* [HLE BIOS] SWIs 0x10-0x2A

## Requirements
* Any modern 64 bit linux distribution (may work on 32 bit but not tested)
* `cmake`, `make`, SDL2 development libraries, Gtk3 development libraries (for `nativefiledialog-extended`), and a compiler that supports C++20

## Usage Instructions
Download the executable from Releases or compile:
* `git clone https://github.com/KellanClark/ecnavdA-yoBemaG.git --recurse-submodules`
* `cd ecnavdA-yoBemaG`
* `mkdir build`
* `cd build`
* `cmake ..`
* `make`

Run the executable named `ecnavda-yobemag`. If the first argument is not valid, it is treated as the ROM path.

Files can also be selected from the "File" menu in the GUI.

Arguments:
* `--rom <file>`
* `--bios <file>` Give path to the BIOS. If invalid or not specified, the emulator will default to an HLE implementation.
* `--record <file.wav>` Record all played audio samples to a WAV file.
* `--uncap-fps` Tries to run the emulator at the maximum possible speed.
