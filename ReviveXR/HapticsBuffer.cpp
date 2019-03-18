#include "HapticsBuffer.h"

HapticsBuffer::HapticsBuffer()
	: m_ReadIndex(0)
	, m_WriteIndex(0)
	, m_Buffer()
{
}

void HapticsBuffer::AddSamples(const ovrHapticsBuffer* buffer)
{
	// Force constant vibration off
	m_ConstantTimeout = 0;

	uint8_t* samples = (uint8_t*)buffer->Samples;

	for (int i = 0; i < buffer->SamplesCount; i++)
	{
		if (m_WriteIndex == m_ReadIndex - 1)
			return;

		m_Buffer[m_WriteIndex] = samples[i];

		// We increment the atomic here as a memory barrier
		// Index will overflow correctly, so no need for a modulo operator
		m_WriteIndex++;
	}
}

float HapticsBuffer::GetSample()
{
	if (m_ConstantTimeout > 0)
	{
		m_ConstantMutex.lock();
		float sample = m_Amplitude;
		if (m_Frequency <= 0.5f && m_ConstantTimeout % 2 == 0)
			sample = 0.0f;
		m_ConstantMutex.unlock();

		m_ConstantTimeout--;
		return sample;
	}

	// We can't pass the write index, so the buffer is now empty
	if (m_ReadIndex == m_WriteIndex)
		return 0.0f;

	uint8_t sample = m_Buffer[m_ReadIndex];

	// We increment the atomic here as a memory barrier
	// Index will overflow correctly, so no need for a modulo operator
	m_ReadIndex++;

	return sample / 255.0f;
}

ovrHapticsPlaybackState HapticsBuffer::GetState()
{
	ovrHapticsPlaybackState state = { 0 };

	for (uint8_t i = m_WriteIndex; i != m_ReadIndex; i++)
		state.RemainingQueueSpace++;

	for (uint8_t i = m_ReadIndex; i != m_WriteIndex; i++)
		state.SamplesQueued++;

	return state;
}
