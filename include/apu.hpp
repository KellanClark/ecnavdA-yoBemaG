
#ifndef GBA_APU
#define GBA_APU

#include "types.hpp"
#include <array>

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

	int sampleRate;
	int sampleBufferIndex;
	std::array<i16, 2048> sampleBuffer;

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
				u32 frequency : 11;
				u32 : 3;
				u32 consecutiveSelection : 1;
				u32 initial : 1;
				u32 : 16;
			};
			u32 SOUND1CNT_X; // 0x4000064
		};
		bool dacOn;
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
				u32 frequency : 11;
				u32 : 3;
				u32 consecutiveSelection : 1;
				u32 initial : 1;
				u32 : 16;
			};
			u32 SOUND2CNT_H; // 0x400006C
		};
		bool dacOn;
		int frequencyTimer;
		unsigned int waveIndex;
		int lengthCounter;
		int periodTimer;
		int currentVolume;
	} channel2;
	struct {
		union {
			struct {
				u16 out1Volume: 3;
				u16 : 1;
				u16 out2Volume : 3;
				u16 : 1;
				u16 ch1out1 : 1;
				u16 ch2out1 : 1;
				u16 ch3out1 : 1;
				u16 ch4out1 : 1;
				u16 ch1out2 : 1;
				u16 ch2out2 : 1;
				u16 ch3out2 : 1;
				u16 ch4out2 : 1;
			};
			u16 SOUNDCNT_L; // 0x4000080
		};
		union {
			struct {
				u16 psgVolume : 2;
				u16 chAVolume : 1;
				u16 chBVolume : 1;
				u16 : 4;
				u16 chAout1 : 1;
				u16 chAout2 : 1;
				u16 chATimer : 1;
				u16 chAReset : 1;
				u16 chBout1 : 1;
				u16 chBout2 : 1;
				u16 chBTimer : 1;
				u16 chBReset : 1;
			};
			u16 SOUNDCNT_H; // 0x4000082
		};
		union {
			struct {
				u32 ch1On : 1;
				u32 ch2On : 1;
				u32 ch3On : 1;
				u32 ch4On : 1;
				u32 : 3;
				u32 allOn : 1;
				u32 : 24;
			};
			u32 SOUNDCNT_X; // 0x4000084
		};
		union {
			struct {
				u32 : 1;
				u32 biasLevel : 9;
				u32 : 4;
				u32 soundResolution : 2;
				u32 : 16;
			};
			u32 SOUNDBIAS; // 0x4000088
		};
		float volumeFloat1;
		float volumeFloat2;
	} soundControl;
	struct {
		u32 FIFO_A; // 0x40000A0
		u32 FIFO_AL;
		int samplesInFifo;
		float currentSample;
	} channelA;
	struct {
		u64 FIFO_B; // 0x40000A4
		u64 FIFO_BL;
		int samplesInFifo;
		float currentSample;
	} channelB;
};

#endif