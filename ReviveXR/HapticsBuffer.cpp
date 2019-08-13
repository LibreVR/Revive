#include "HapticsBuffer.h"

HapticsBuffer::HapticsBuffer()
	: m_ReadIndex(0xFF)
	, m_WriteIndex(0)
	, m_Buffer()
{
}

void HapticsBuffer::AddSamples(const ovrHapticsBuffer* buffer)
{
	uint8_t* samples = (uint8_t*)buffer->Samples;

	for (int i = 0; i < buffer->SamplesCount; i++)
	{
		if (m_WriteIndex == m_ReadIndex)
			return;

		m_Buffer[m_WriteIndex] = samples[i];

		// We increment the atomic here as a memory barrier
		// Index will overflow correctly, so no need for a modulo operator
		m_WriteIndex++;
	}
}

float HapticsBuffer::GetSample()
{
	// We can't pass the write index, so the buffer is now empty
	if (m_ReadIndex + 1 == m_WriteIndex)
		return 0.0f;

	// Index will overflow correctly, so no need for a modulo operator
	uint8_t sample = m_Buffer[++m_ReadIndex];

	return sample / 255.0f;
}

ovrHapticsPlaybackState HapticsBuffer::GetState()
{
	ovrHapticsPlaybackState state = { 0 };

	for (uint8_t i = m_WriteIndex; i != m_ReadIndex; i++)
		state.RemainingQueueSpace++;
	state.RemainingQueueSpace++;

	for (uint8_t i = m_ReadIndex; i != m_WriteIndex; i++)
		state.SamplesQueued++;
	state.SamplesQueued--;

	return state;
}
