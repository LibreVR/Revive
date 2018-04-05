#pragma once

#include "OVR_CAPI.h"

#include <memory>
#include <atomic>

#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Perception.Spatial.h>
#include <winrt/Windows.UI.Input.Spatial.h>
#include <winrt/Windows.Foundation.h>

// FWD-decl
class FrameList;
class CompositorWGL;
class Win32Window;
class TrackingManager;

struct ovrHmdStruct
{
	ovrTrackingOrigin Origin = ovrTrackingOrigin_EyeLevel;
	winrt::Windows::Graphics::Holographic::HolographicSpace Space = nullptr;
	winrt::Windows::UI::Input::Spatial::SpatialInteractionManager Interaction = nullptr;

	std::unique_ptr<FrameList> Frames = nullptr;
	std::unique_ptr<CompositorWGL> Compositor = nullptr;
	std::unique_ptr<Win32Window> Window = nullptr;
	std::unique_ptr<TrackingManager> Tracking = nullptr;

	std::atomic_uint32_t ConnectedControllers = 0;
};
