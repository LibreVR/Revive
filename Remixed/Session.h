#pragma once

#include "OVR_CAPI.h"

#include <memory>

#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Perception.Spatial.h>

// FWD-decl
class FrameList;
class CompositorWGL;
class Win32Window;

struct ovrHmdStruct
{
	winrt::Windows::Graphics::Holographic::HolographicSpace Space = nullptr;
	winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference Reference = nullptr;

	std::unique_ptr<FrameList> Frames = nullptr;
	std::unique_ptr<CompositorWGL> Compositor = nullptr;
	std::unique_ptr<Win32Window> Window = nullptr;
};
