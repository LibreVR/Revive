#include "HapticsBuffer.h"

HapticsBuffer::HapticsBuffer()
	: m_ReadIndex(0)
	, m_WriteIndex(0)
{
	memset(m_Buffer, 0, sizeof(m_Buffer));
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

		// Index will overflow correctly, so no need for a modulo operator
		m_Buffer[m_WriteIndex++] = samples[i];
	}
}

void HapticsBuffer::SetConstant(float frequency, float amplitude)
{
	// The documentation specifies a constant vibration should time out after 2.5 seconds
	m_Amplitude = amplitude;
	m_Frequency = frequency;
	m_ConstantTimeout = (uint16_t)(REV_HAPTICS_SAMPLE_RATE * 2.5);
}

float HapticsBuffer::GetSample()
{
	if (m_ConstantTimeout > 0)
	{
		float sample = m_Amplitude;
		if (m_Frequency <= 0.5f && m_ConstantTimeout % 2 == 0)
			sample = 0.0f;

		m_ConstantTimeout--;
		return sample;
	}

	// We can't pass the write index, so the buffer is now empty
	if (m_ReadIndex == m_WriteIndex)
		return 0.0f;

	// Index will overflow correctly, so no need for a modulo operator
	return m_Buffer[m_ReadIndex++] / 255.0f;
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
