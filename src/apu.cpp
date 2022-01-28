
#include "apu.hpp"
#include "scheduler.hpp"
#include "gba.hpp"
#include <cstdio>

GBAAPU::GBAAPU(GameBoyAdvance& bus_) : bus(bus_) {
	ch1OverrideEnable = ch2OverrideEnable = ch3OverrideEnable = ch4OverrideEnable = chAOverrideEnable = chBOverrideEnable = true;

	reset();
}

void GBAAPU::reset() {
	frameSequencerCounter = 0;

	channelA.fifo = {};
	channelB.fifo = {};
	channelB.currentSample = 0;
	soundControl.chBReset = false;

	systemEvents.addEvent(16777216 / 32768, sampleEvent, this);
}

static const float squareWaveDutyCycles[4][8] {
	{1, 0, 0, 0, 0, 0, 0, 0}, // 12.5%
	{1, 1, 0, 0, 0, 0, 0, 0}, // 25%
	{1, 1, 1, 1, 0, 0, 0, 0}, // 50%
	{1, 1, 1, 1, 1, 1, 0, 0}  // 75%
};

void GBAAPU::frameSequencerEvent(void *object) {
	static_cast<GBAAPU *>(object)->tickFrameSequencer();
}

int GBAAPU::calculateSweepFrequency() {
	int newFrequency = channel1.shadowFrequency >> channel1.sweepShift;

	if (channel1.sweepDecrease) {
		newFrequency = channel1.shadowFrequency - newFrequency;
	} else {
		newFrequency = channel1.shadowFrequency + newFrequency;
	}

	if (newFrequency > 2047)
		soundControl.ch1On = false;

	return newFrequency;
}

void GBAAPU::tickFrameSequencer() {
	return;
	++frameSequencerCounter;

	// Tick Length
	if (!(frameSequencerCounter & 1)) {
		if (channel1.consecutiveSelection) {
			if (channel1.lengthCounter) {
				if ((--channel1.lengthCounter) == 0) {
					soundControl.ch1On = false;
				}
			}
		}
		if (channel2.consecutiveSelection) {
			if (channel2.lengthCounter) {
				if ((--channel2.lengthCounter) == 0) {
					soundControl.ch2On = false;
				}
			}
		}
	}
	// Tick Volume
	if ((frameSequencerCounter & 7) == 7) {
		if (channel1.periodTimer) {
			if ((--channel1.periodTimer) == 0) {
				channel1.periodTimer = channel1.envelopeSweepNum;
				if (channel1.periodTimer == 0) {
					channel1.periodTimer = 8;
				}
				if ((channel1.currentVolume < 0xF) && channel1.envelopeIncrease) {
					++channel1.currentVolume;
				} else if ((channel1.currentVolume > 0) && !channel1.envelopeIncrease) {
					--channel1.currentVolume;
				}
			}
		}
		if (channel2.periodTimer) {
			if ((--channel2.periodTimer) == 0) {
				channel2.periodTimer = channel2.envelopeSweepNum;
				if (channel2.periodTimer == 0) {
					channel2.periodTimer = 8;
				}
				if ((channel2.currentVolume < 0xF) && channel2.envelopeIncrease) {
					++channel2.currentVolume;
				} else if ((channel2.currentVolume > 0) && !channel2.envelopeIncrease) {
					--channel2.currentVolume;
				}
			}
		}
	}
	// Tick Sweep
	if ((frameSequencerCounter & 3) == 2) {
		if (channel1.sweepTimer) {
			if ((--channel1.sweepTimer) == 0) {
				channel1.sweepTimer = channel1.sweepTime ? channel1.sweepTime : 8;

				if (channel1.sweepEnabled && channel1.sweepTime) {
					int calculatedNewFrequency = calculateSweepFrequency();
					if ((calculatedNewFrequency < 2048) && channel1.sweepShift) {
						channel1.frequency = calculatedNewFrequency;
						channel1.shadowFrequency = calculatedNewFrequency;

						calculateSweepFrequency();
					}
				}
			}
		}
	}

	systemEvents.addEvent(8192 * 4, frameSequencerEvent, this);
}

void GBAAPU::sampleEvent(void *object) {
	static_cast<GBAAPU *>(object)->generateSample();
}

void GBAAPU::generateSample() {
	for (int i = 0; i < (16777216 / 32768) / 4; i++) {
		if (--channel1.frequencyTimer <= 0) {
			channel1.frequencyTimer = (2048 - (channel1.frequency * 4));
			channel1.waveIndex = (channel1.waveIndex + 1) & 7;
		}
		if (--channel2.frequencyTimer <= 0) {
			channel2.frequencyTimer = (2048 - (channel2.frequency * 4));
			channel2.waveIndex = (channel2.waveIndex + 1) & 7;
		}
	}

	float ch1Sample = ch1OverrideEnable * soundControl.ch1On * channel1.dacOn * (((channel1.currentVolume * squareWaveDutyCycles[channel1.waveDuty][channel1.waveIndex]) / 7.5) - 1.0f) * ((float)(soundControl.psgVolume + 1) / 4);
	i16 ch1SampleR = ch1Sample * soundControl.ch1outR * soundControl.volumeFloatR * 0x7F;
	i16 ch1SampleL = ch1Sample * soundControl.ch1outL * soundControl.volumeFloatL * 0x7F;
	float ch2Sample = ch2OverrideEnable * soundControl.ch2On * channel2.dacOn * (((channel2.currentVolume * squareWaveDutyCycles[channel2.waveDuty][channel2.waveIndex]) / 7.5) - 1.0f) * ((float)(soundControl.psgVolume + 1) / 4);
	i16 ch2SampleR = ch2Sample * soundControl.ch2outR * soundControl.volumeFloatR * 0x7F;
	i16 ch2SampleL = ch2Sample * soundControl.ch2outL * soundControl.volumeFloatL * 0x7F;
	float ch3Sample = ch3OverrideEnable * 0;
	i16 ch3SampleR = ch3Sample * soundControl.ch3outR * soundControl.volumeFloatR * 0x7F;
	i16 ch3SampleL = ch3Sample * soundControl.ch3outL * soundControl.volumeFloatL * 0x7F;
	float ch4Sample = ch4OverrideEnable * 0;
	i16 ch4SampleR = ch4Sample * soundControl.ch4outR * soundControl.volumeFloatR * 0x7F;
	i16 ch4SampleL = ch4Sample * soundControl.ch4outL * soundControl.volumeFloatL * 0x7F;
	float chASample = chAOverrideEnable * (channelA.currentSample * ((float)(soundControl.chAVolume + 1) / 2));
	i16 chASampleR = chASample * soundControl.chAoutR * 0x1FF;
	i16 chASampleL = chASample * soundControl.chAoutL * 0x1FF;
	//printf("%d, %d\n", soundControl.chAoutR, soundControl.chAoutL);
	float chBSample = chBOverrideEnable * (channelB.currentSample * ((float)(soundControl.chBVolume + 1) / 2));
	i16 chBSampleR = chBSample * soundControl.chBoutR * 0x1FF;
	i16 chBSampleL = chBSample * soundControl.chBoutL * 0x1FF;
	//printf("%d, %d\n%d, %d\n\n", chASampleR, chASampleL, chBSampleR, chBSampleL);
	//printf("%f  %f\n", channelA.currentSample, channelB.currentSample);

	if (soundControl.allOn) {
		sampleBuffer[sampleBufferIndex] = std::clamp(ch1SampleR + ch2SampleR + ch3SampleR + ch4SampleR + chASampleR + chBSampleR + soundControl.biasLevel, 0, 0x3FF);
		sampleBuffer[sampleBufferIndex] = ((sampleBuffer[sampleBufferIndex] << 6) | (sampleBuffer[sampleBufferIndex] >> 4)) - 0x8000;
		sampleBufferIndex++;
		sampleBuffer[sampleBufferIndex] = std::clamp(ch1SampleL + ch2SampleL + ch3SampleL + ch4SampleL + chASampleL + chBSampleL + soundControl.biasLevel, 0, 0x3FF);
		sampleBuffer[sampleBufferIndex] = ((sampleBuffer[sampleBufferIndex] << 6) | (sampleBuffer[sampleBufferIndex] >> 4)) - 0x8000;
		sampleBufferIndex++;
		//printf("%d,     %d\n", sampleBuffer[sampleBufferIndex - 2], sampleBuffer[sampleBufferIndex - 1]);
	} else {
		sampleBuffer[sampleBufferIndex++] = ((soundControl.biasLevel << 6) | (soundControl.biasLevel >> 4)) - 0x8000;
		sampleBuffer[sampleBufferIndex++] = ((soundControl.biasLevel << 6) | (soundControl.biasLevel >> 4)) - 0x8000;
	}
	//sampleBuffer[sampleBufferIndex++] = (sampleBufferIndex & 8) ? 0x7FFF : 0;
	//sampleBuffer[sampleBufferIndex++] = (sampleBufferIndex & 8) ? 0x7FFF : 0;

	if (sampleBufferIndex == (sizeof(sampleBuffer) / sizeof(i16)))
		bus.cpu.running = false;

	systemEvents.addEvent(16777216 / 32768, sampleEvent, this);
}

void GBAAPU::onTimer(int timerNum) {
	if (soundControl.chATimer == timerNum) {
		// Get new sample
		if (channelA.fifo.size()) {
			channelA.currentSample = (float)channelA.fifo.front() / 128;
			channelA.fifo.pop();
		} else {
			channelA.currentSample = 0;
		}

		// Request new data
		if (channelA.fifo.size() <= 16)
			bus.dma.onFifoA();
	}
	if (soundControl.chBTimer == timerNum) {
		// Get new sample
		if (channelB.fifo.size()) {
			channelB.currentSample = (float)channelB.fifo.front() / 128;
			channelB.fifo.pop();
		} else {
			channelB.currentSample = 0;
		}

		// Request new data
		if (channelB.fifo.size() <= 16)
			bus.dma.onFifoB();
	}
}

u8 GBAAPU::readIO(u32 address) {
	switch (address) {
	case 0x4000060:
		return (u8)channel1.SOUND1CNT_L;
	case 0x4000061:
		return (u8)(channel1.SOUND1CNT_L >> 8);
	case 0x4000062:
		return (u8)(channel1.SOUND1CNT_H & 0xC0);
	case 0x4000063:
		return (u8)(channel1.SOUND1CNT_H >> 8);
	case 0x4000064:
		return (u8)(channel1.SOUND1CNT_X & 0);
	case 0x4000065:
		return (u8)((channel1.SOUND1CNT_X >> 8) & 0x40);
	case 0x4000066:
	case 0x4000067:
		return 0;
	case 0x4000068:
		return (u8)channel2.SOUND2CNT_L;
	case 0x4000069:
		return (u8)(channel2.SOUND2CNT_L >> 8);
	case 0x400006A:
	case 0x400006B:
		return 0;
	case 0x400006C:
		return (u8)channel2.SOUND2CNT_H;
	case 0x400006D:
		return (u8)(channel2.SOUND2CNT_H >> 8);
	case 0x400006E:
	case 0x400006F:
		return 0;
	case 0x4000080:
		return (u8)soundControl.SOUNDCNT_L;
	case 0x4000081:
		return (u8)(soundControl.SOUNDCNT_L >> 8);
	case 0x4000082:
		return (u8)soundControl.SOUNDCNT_H;
	case 0x4000083:
		return (u8)(soundControl.SOUNDCNT_H >> 8);
	case 0x4000084:
		return (u8)soundControl.SOUNDCNT_X;
	case 0x4000085:
		return (u8)(soundControl.SOUNDCNT_X >> 8);
	case 0x4000086:
	case 0x4000087:
		return 0;
	case 0x4000088:
		return (u8)soundControl.SOUNDBIAS;
	case 0x4000089:
		return (u8)(soundControl.SOUNDBIAS >> 8);
	case 0x400008A:
	case 0x400008B:
		return 0;
	default:
		return bus.openBus<u8>(address);
	}
}

void GBAAPU::writeIO(u32 address, u8 value) {
	switch (address) {
	case 0x4000060:
		channel1.SOUND1CNT_L = value & 0x7F;
		break;
	case 0x4000062:
		channel1.SOUND1CNT_H = (channel1.SOUND1CNT_H & 0xFF00) | value;
		channel1.lengthCounter = 64 - channel1.soundLength;
		break;
	case 0x4000063:
		channel1.SOUND1CNT_H = (channel1.SOUND1CNT_H & 0x00FF) | (value << 8);
		channel1.dacOn = (value & 0xF8) != 0;
		if (!channel1.dacOn)
			soundControl.ch1On = false;
		break;
	case 0x4000064:
		channel1.SOUND1CNT_X = (channel1.SOUND1CNT_X & 0xFF00) | value;
		channel1.shadowFrequency = channel1.frequency;
		break;
	case 0x4000065:
		if (value & 0x80) {
			channel1.sweepTimer = channel1.sweepTime ? channel1.sweepTime : 8;
			if (channel1.sweepTime || channel1.sweepShift)
				channel1.sweepEnabled = true;
			if (channel1.sweepShift)
				calculateSweepFrequency();
			if (!channel1.lengthCounter) {
				channel1.lengthCounter = 64;
			}
			channel1.periodTimer = channel1.envelopeSweepNum;
			channel1.currentVolume = channel1.envelopeStartVolume;
			if (channel1.dacOn)
				soundControl.ch1On = true;
		}

		channel1.SOUND1CNT_X = (channel1.SOUND1CNT_X & 0x00FF) | ((value & 0xC0) << 8);
		channel1.shadowFrequency = channel1.frequency;
		break;
	case 0x4000068:
		channel2.SOUND2CNT_L = value;
		channel2.lengthCounter = 64 - channel2.soundLength;
		break;
	case 0x4000069:
		channel2.SOUND2CNT_L = (channel2.SOUND2CNT_L & 0x00FF) | (value << 8);
		channel2.dacOn = (value & 0xF8) != 0;
		if (!channel2.dacOn)
			soundControl.ch2On = false;
		break;
	case 0x400006C:
		channel2.SOUND2CNT_H = (channel2.SOUND2CNT_H & 0xFF00) | value;
		break;
	case 0x400006B:
		if (value & 0x80) {
			if (!channel2.lengthCounter)
				channel2.lengthCounter = 64;
			channel2.periodTimer = channel2.envelopeSweepNum;
			channel2.currentVolume = channel2.envelopeStartVolume;
			if (channel2.dacOn)
				soundControl.ch2On = true;
		}

		channel2.SOUND2CNT_H = (channel2.SOUND2CNT_H & 0x00FF) | (value << 8);
		break;
	case 0x4000080:
		soundControl.SOUNDCNT_L = (soundControl.SOUNDCNT_L & 0xFF00) | value;
		soundControl.volumeFloatR = (float)soundControl.outRVolume / 7;
		soundControl.volumeFloatL = (float)soundControl.outLVolume / 7;
		break;
	case 0x4000081:
		soundControl.SOUNDCNT_L = (soundControl.SOUNDCNT_L & 0x00FF) | (value << 8);
		break;
	case 0x4000082:
		soundControl.SOUNDCNT_H = (soundControl.SOUNDCNT_H & 0xFF00) | value;
		break;
	case 0x4000083:
		soundControl.SOUNDCNT_H = (soundControl.SOUNDCNT_H & 0x00FF) | (value << 8);
		
		if (soundControl.chAReset) {
			channelA.fifo = {};
			channelA.currentSample = 0;

			soundControl.chAReset = false;
		}
		if (soundControl.chBReset) {
			channelB.fifo = {};
			channelB.currentSample = 0;

			soundControl.chBReset = false;
		}
		break;
	case 0x4000084:
		soundControl.SOUNDCNT_X = (value & 0x80);
		break;
	case 0x4000088:
		soundControl.SOUNDBIAS = (soundControl.SOUNDBIAS & 0xFF00) | value;
		break;
	case 0x4000089:
		soundControl.SOUNDBIAS = (soundControl.SOUNDBIAS & 0x00FF) | (value << 8);
		break;
	case 0x40000A0:
	case 0x40000A1:
	case 0x40000A2:
	case 0x40000A3:
		if (channelA.fifo.size() < 32)
			channelA.fifo.push((i8)value);
		break;
	case 0x40000A4:
	case 0x40000A5:
	case 0x40000A6:
	case 0x40000A7:
		if (channelB.fifo.size() < 32)
			channelB.fifo.push((i8)value);
		break;
	}
}