#pragma once

#include "OVR_CAPI.h"

#include <memory>

#include <winrt/Windows.Graphics.Holographic.h>
using namespace winrt::Windows::Graphics::Holographic;

// FWD-decl
class CompositorBase;

struct ovrHmdStruct
{
	HolographicSpace Space = nullptr;
	HolographicFrame Frame = nullptr;

	std::unique_ptr<CompositorBase> Compositor = nullptr;
};
