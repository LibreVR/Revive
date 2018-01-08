#pragma once

#include "OVR_CAPI.h"

#include <memory>
#include <vector>

#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Perception.h>
#include <winrt/Windows.Perception.Spatial.h>

// FWD-decl
class CompositorBase;

struct ovrHmdStruct
{
	using Frame = std::pair<long long, winrt::Windows::Graphics::Holographic::HolographicFrame>;
	std::vector<Frame> Frames;
	winrt::Windows::Graphics::Holographic::HolographicFrame CurrentFrame = nullptr;
	winrt::Windows::Graphics::Holographic::HolographicSpace Space = nullptr;
	winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference Reference = nullptr;

	std::unique_ptr<CompositorBase> Compositor = nullptr;

	winrt::Windows::Graphics::Holographic::HolographicFrame GetFrameFromIndex(long long frameIndex)
	{
		long long first_index = Frames.front().first;
		long long last_index = Frames.back().first;
		if (last_index < frameIndex)
		{
			for (long long i = last_index + 1; i <= frameIndex; i++)
				Frames.push_back(Frame(i, Space.CreateNextFrame()));
			return Frames.back().second;
		}
		else if (frameIndex < first_index)
		{
			return nullptr;
		}
		else
		{
			return Frames[frameIndex - first_index].second;
		}
	}
};
