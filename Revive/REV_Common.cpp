#include "REV_Common.h"

#include <stdio.h>

// Common structures

ovrTextureSwapChainData::ovrTextureSwapChainData(vr::EGraphicsAPIConvention api, ovrTextureSwapChainDesc desc)
	: ApiType(api)
	, Length(REV_SWAPCHAIN_LENGTH)
	, CurrentIndex(0)
	, Submitted(nullptr)
	, Desc(desc)
	, Overlay(vr::k_ulOverlayHandleInvalid)
{
	memset(Textures, 0, sizeof(Textures));
}

ovrMirrorTextureData::ovrMirrorTextureData(vr::EGraphicsAPIConvention api, ovrMirrorTextureDesc desc)
	: ApiType(api)
	, Desc(desc)
	, Target(nullptr)
	, Shader(nullptr)
{
	memset(Views, 0, sizeof(Views));
}

ovrHmdStruct::ovrHmdStruct()
	: ShouldQuit(false)
	, IsVisible(false)
	, FrameIndex(0)
	, Compositor(nullptr)
	, Input(new InputManager())
{
}

ovrHmdStruct::~ovrHmdStruct()
{
	delete Input;
	delete Compositor;
}

// Common functions

unsigned int rev_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose)
{
	unsigned int result = 0;

	if (pose.bPoseIsValid)
	{
		if (pose.bDeviceIsConnected)
			result |= ovrStatus_OrientationTracked;
		if (pose.eTrackingResult != vr::TrackingResult_Calibrating_OutOfRange &&
			pose.eTrackingResult != vr::TrackingResult_Running_OutOfRange)
			result |= ovrStatus_PositionTracked;
	}

	return result;
}
