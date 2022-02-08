
#ifndef GBA_APU
#define GBA_APU

#include "types.hpp"
#include <array>
#include <queue>
#include <atomic>

class GameBoyAdvance;
class GBAAPU {
public:
	GameBoyAdvance& bus;

	GBAAPU(GameBoyAdvance& bus_);
	void reset();

	int calculateSweepFrequency();

	static void frameSequencerEvent(void *object);
	void tickFrameSequencer();
	static void sampleEvent(void *object);
	void generateSample();

	void onTimer(int timerNum);

    u8 readIO(u32 address);
	void writeIO(u32 address, u8 value);

	std::mutex sampleBufferMutex;
	std::atomic<int> sampleBufferIndex;
	std::array<i16, 2048> sampleBuffer;
	bool apuBlock;

	bool ch1OverrideEnable;
	bool ch2OverrideEnable;
	bool ch3OverrideEnable;
	bool ch4OverrideEnable;
	bool chAOverrideEnable;
	bool chBOverrideEnable;
	int frameSequencerCounter;

	struct {
		union {
			struct {
				u16 sweepShift : 3;
				u16 sweepDecrease : 1;
				u16 sweepTime : 3;
				u16 : 9;
			};
			u16 SOUND1CNT_L; // 0x4000060
		};
		union {
			struct {
				u16 soundLength : 6;
				u16 waveDuty : 2;
				u16 envelopeSweepNum : 3;
				u16 envelopeIncrease : 1;
				u16 envelopeStartVolume : 4;
			};
			u16 SOUND1CNT_H; // 0x4000062
		};
		union {
			struct {
				u16 frequency : 11;
				u16 : 3;
				u16 consecutiveSelection : 1;
				u16 initial : 1;
			};
			u16 SOUND1CNT_X; // 0x4000064
		};
		int frequencyTimer;
		unsigned int waveIndex;
		bool sweepEnabled;
		int shadowFrequency;
		int sweepTimer;
		int lengthCounter;
		int periodTimer;
		int currentVolume;
	} channel1;
	struct {
		union {
			struct {
				u16 soundLength : 6;
				u16 waveDuty : 2;
				u16 envelopeSweepNum : 3;
				u16 envelopeIncrease : 1;
				u16 envelopeStartVolume : 4;
			};
			u16 SOUND2CNT_L; // 0x4000068
		};
		union {
			struct {
				u16 frequency : 11;
				u16 : 3;
				u16 consecutiveSelection : 1;
				u16 initial : 1;
			};
			u16 SOUND2CNT_H; // 0x400006C
		};
		int frequencyTimer;
		unsigned int waveIndex;
		int lengthCounter;
		int periodTimer;
		int currentVolume;
	} channel2;
	struct {
		union {
			struct {
				u16 : 5;
				u16 dimension : 1;
				u16 selectedBank : 1;
				u16 dacOn : 1;
				u16 : 8;
			};
			u16 SOUND3CNT_L; // 0x4000070
		};
		union {
			struct {
				u16 soundLength : 8;
				u16 : 5;
				u16 volume : 2;
				u16 forceVolume : 1;
			};
			u16 SOUND3CNT_H; // 0x4000072
		};
		union {
			struct {
				u16 frequency : 11;
				u16 : 3;
				u16 consecutiveSelection : 1;
				u16 initial : 1;
			};
			u16 SOUND3CNT_X; // 0x4000074
		};
		u8 waveMem[32 * 2]; // 0x4000090 - 0x400009F
		unsigned int waveMemIndex;
		int frequencyTimer;
		int lengthCounter;
	} channel3;
	struct {
		union {
			struct {
				u16 soundLength : 6;
				u16 : 2;
				u16 envelopeSweepNum : 3;
				u16 envelopeIncrease : 1;
				u16 envelopeStartVolume : 4;
			};
			u16 SOUND4CNT_L; // 0x4000078
		};
		union {
			struct {
				u16 divideRatio : 3;
				u16 counterWidth : 1;
				u16 shiftClockFrequency : 4;
				u16 : 6;
				u16 consecutiveSelection : 1;
				u16 initial : 1;
			};
			u16 SOUND4CNT_H; // 0x400007C
		};
		int frequencyTimer;
		u16 lfsr;
		int lengthCounter;
		int periodTimer;
		int currentVolume;
	} channel4;
	struct {
		union {
			struct {
				u16 outRVolume: 3;
				u16 : 1;
				u16 outLVolume : 3;
				u16 : 1;
				u16 ch1outR : 1;
				u16 ch2outR : 1;
				u16 ch3outR : 1;
				u16 ch4outR : 1;
				u16 ch1outL : 1;
				u16 ch2outL : 1;
				u16 ch3outL : 1;
				u16 ch4outL : 1;
			};
			u16 SOUNDCNT_L; // 0x4000080
		};
		union {
			struct {
				u16 psgVolume : 2;
				u16 chAVolume : 1;
				u16 chBVolume : 1;
				u16 : 4;
				u16 chAoutR : 1;
				u16 chAoutL : 1;
				u16 chATimer : 1;
				u16 chAReset : 1;
				u16 chBoutR : 1;
				u16 chBoutL : 1;
				u16 chBTimer : 1;
				u16 chBReset : 1;
			};
			u16 SOUNDCNT_H; // 0x4000082
		};
		union {
			struct {
				u16 ch1On : 1;
				u16 ch2On : 1;
				u16 ch3On : 1;
				u16 ch4On : 1;
				u16 : 3;
				u16 allOn : 1;
				u16 : 8;
			};
			u16 SOUNDCNT_X; // 0x4000084
		};
		union {
			struct {
				u16 biasLevel : 10;
				u16 : 4;
				u16 soundResolution : 2;
			};
			u16 SOUNDBIAS; // 0x4000088
		};
		float volumeFloatR;
		float volumeFloatL;
	} soundControl;
	struct {
		std::queue<i8> fifo; // 0x40000A0
		float currentSample;
	} channelA;
	struct {
		std::queue<i8> fifo; // 0x40000A4
		float currentSample;
	} channelB;
};

#endif