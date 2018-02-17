#pragma once

#include <list>
#include <shared_mutex>

#include <winrt/Windows.Graphics.Holographic.h>

class FrameList
{
public:
	FrameList(winrt::Windows::Graphics::Holographic::HolographicSpace space);
	~FrameList() {};

	winrt::Windows::Graphics::Holographic::HolographicFrame GetFrame(long long frameIndex = -1);
	winrt::Windows::Graphics::Holographic::HolographicFrame GetFrameAtTime(double absTime);
	winrt::Windows::Graphics::Holographic::HolographicCameraPose GetPose(long long frameIndex = -1, uint32_t displayIndex = 0);
	winrt::Windows::Graphics::Holographic::HolographicStereoTransform GetLocalViewTransform(long long frameIndex = -1, uint32_t displayIndex = 0);

	void BeginFrame(long long frameIndex);
	void EndFrame(long long frameIndex);
	void Clear();

private:
	winrt::Windows::Graphics::Holographic::HolographicSpace m_space;
	std::shared_mutex m_frame_mutex;
	using Frame = std::pair<long long, winrt::Windows::Graphics::Holographic::HolographicFrame>;
	std::list<Frame> m_frames;
	long long m_last_index = -1;
};