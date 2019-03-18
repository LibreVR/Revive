#pragma once

#include "OVR_CAPI.h"

#include <atomic>
#include <mutex>

#define REV_HAPTICS_SAMPLE_RATE 320

class HapticsBuffer
{
public:
	HapticsBuffer();
	~HapticsBuffer() { }

	void AddSamples(const ovrHapticsBuffer* buffer);
	float GetSample();
	ovrHapticsPlaybackState GetState();

private:
	// Lock-less circular buffer, single producer/consumer
	std::atomic_uint8_t m_ReadIndex;
	std::atomic_uint8_t m_WriteIndex;
	uint8_t m_Buffer[OVR_HAPTICS_BUFFER_SAMPLES_MAX];

	// Constant feedback, uses locks
	std::mutex m_ConstantMutex;
	std::atomic_uint16_t m_ConstantTimeout;
	float m_Frequency;
	float m_Amplitude;
};

static_assert(OVR_HAPTICS_BUFFER_SAMPLES_MAX == 256, "The Haptics Buffer is designed for 256 samples");
