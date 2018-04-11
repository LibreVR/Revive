#pragma once

#include <list>
#include <shared_mutex>
#include <atomic>

#include <winrt/Windows.Graphics.Holographic.h>

class FrameList
{
public:
	FrameList(winrt::Windows::Graphics::Holographic::HolographicSpace space);
	~FrameList() {};

	winrt::Windows::Graphics::Holographic::HolographicFrame GetFrame(long long frameIndex = 0);
	winrt::Windows::Graphics::Holographic::HolographicFrame GetPendingFrame(long long frameIndex = 0);
	winrt::Windows::Graphics::Holographic::HolographicFrame GetFrameAtTime(double absTime);
	winrt::Windows::Graphics::Holographic::HolographicCameraPose GetPose(long long frameIndex = 0, uint32_t displayIndex = 0);

	void BeginFrame(long long frameIndex = 0);
	void EndFrame(long long frameIndex = 0);
	void Clear();

private:
	winrt::Windows::Graphics::Holographic::HolographicSpace m_space;

	using Frame = std::pair<long long, winrt::Windows::Graphics::Holographic::HolographicFrame>;
	std::list<Frame> m_frames;
	std::shared_mutex m_frame_mutex;
	std::atomic_llong m_next_index;
	std::atomic_llong m_submitted_index;
};