#pragma once

#include "OVR_CAPI.h"

#include <memory>

#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Perception.Spatial.h>
#include <winrt/Windows.Foundation.h>

// FWD-decl
class FrameList;
class CompositorWGL;
class Win32Window;

struct ovrHmdStruct
{
	winrt::Windows::Graphics::Holographic::HolographicSpace Space = nullptr;
	winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference Reference = nullptr;
	winrt::Windows::Foundation::Numerics::float3 OriginPosition = winrt::Windows::Foundation::Numerics::float3::zero();
	winrt::Windows::Foundation::Numerics::quaternion OriginOrientation = winrt::Windows::Foundation::Numerics::quaternion::identity();

	std::unique_ptr<FrameList> Frames = nullptr;
	std::unique_ptr<CompositorWGL> Compositor = nullptr;
	std::unique_ptr<Win32Window> Window = nullptr;
};
