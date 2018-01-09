#include "FrameList.h"
#include "Session.h"

#include <winrt/Windows.Graphics.Holographic.h>
using namespace winrt::Windows::Graphics::Holographic;

#include <winrt/Windows.Perception.h>
using namespace winrt::Windows::Perception;

#include <winrt/Windows.Foundation.h>
using namespace winrt::Windows::Foundation;

FrameList::FrameList(HolographicSpace space)
	: m_space(space)
{
}

HolographicFrame FrameList::GetFrame(long long frameIndex)
{
	if (m_last_index < frameIndex)
	{
		std::unique_lock<std::shared_mutex> lk(m_frame_mutex);
		for (long long i = m_last_index + 1; i <= frameIndex; i++)
			m_frames.push_back(Frame(i, m_space.CreateNextFrame()));
		m_last_index = m_frames.back().first;
		return m_frames.back().second;
	}
	else
	{
		std::shared_lock<std::shared_mutex> lk(m_frame_mutex);
		auto it = m_frames.begin();
		while (it != m_frames.end() && it->first < frameIndex)
			it++;
		return it->second;
	}
}

HolographicFrame FrameList::GetFrameAtTime(double absTime)
{
	std::shared_lock<std::shared_mutex> lk(m_frame_mutex);
	if (m_frames.empty())
		return nullptr;

	DateTime target(TimeSpan((int64_t)(absTime * 1.0e+7)));
	for (Frame frame : m_frames)
	{
		HolographicFramePrediction pred = frame.second.CurrentPrediction();
		PerceptionTimestamp timestamp = pred.Timestamp();
		DateTime time = timestamp.TargetTime();
		TimeSpan duration = frame.second.Duration();
		if (target < time + duration)
			return frame.second;
	}
	return nullptr;
}

void FrameList::PopFrame(long long frameIndex)
{
	std::unique_lock<std::shared_mutex> lk(m_frame_mutex);
	if (m_frames.empty())
		return;

	do m_frames.pop_front(); while (!m_frames.empty() && m_frames.front().first <= frameIndex);
}
