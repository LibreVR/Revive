#pragma once

#include <list>
#include <shared_mutex>

#include <winrt/Windows.Graphics.Holographic.h>

class FrameList
{
public:
	FrameList(winrt::Windows::Graphics::Holographic::HolographicSpace space);
	~FrameList() {};

	winrt::Windows::Graphics::Holographic::HolographicFrame GetFrame(long long frameIndex);
	winrt::Windows::Graphics::Holographic::HolographicFrame GetFrameAtTime(double absTime);
	void PopFrame(long long frameIndex);

private:
	winrt::Windows::Graphics::Holographic::HolographicSpace m_space;
	std::shared_mutex m_frame_mutex;
	using Frame = std::pair<long long, winrt::Windows::Graphics::Holographic::HolographicFrame>;
	std::list<Frame> m_frames;
	long long m_last_index = -1;
};