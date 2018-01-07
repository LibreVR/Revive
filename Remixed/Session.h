#pragma once

#include "OVR_CAPI.h"

#include <memory>

#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Perception.h>
#include <winrt/Windows.Perception.Spatial.h>

// FWD-decl
class CompositorBase;

struct ovrHmdStruct
{
	winrt::Windows::Graphics::Holographic::HolographicSpace Space = nullptr;
	winrt::Windows::Graphics::Holographic::HolographicFrame Frame = nullptr;
	winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference Reference = nullptr;

	std::unique_ptr<CompositorBase> Compositor = nullptr;
};
