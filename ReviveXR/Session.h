#pragma once

#include <OVR_CAPI.h>
#include <openxr/openxr.h>
#include <memory>
#include <utility>
#include <atomic>

#include "SessionDetails.h"

class InputManager;

struct SessionStatusBits {
	bool IsVisible : 1;
	bool HmdPresent : 1;
	bool HmdMounted : 1;
	bool DisplayLost : 1;
	bool ShouldQuit : 1;
	bool ShouldRecenter : 1;
	bool HasInputFocus : 1;
	bool OverlayPresent : 1;
};

typedef struct XrIndexedFrameState : public XrFrameState
{
	long long frameIndex;
} XrIndexedFrameState;

struct ovrHmdStruct
{
	std::pair<void**, void*> HookedFunction;

	// OpenXR handles
	XrInstance Instance;
	XrSystemId System;
	XrSession Session;
	XrSpace ViewSpace;
	XrSpace	LocalSpace;
	XrSpace StageSpace;

	// Frame state
	XrIndexedFrameState FrameStats[ovrMaxProvidedFrameStats];
	std::atomic<XrIndexedFrameState*> CurrentFrame;

	// OpenXR properties
	XrSystemProperties SystemProperties;
	XrReferenceSpaceType TrackingSpace;
	XrViewConfigurationView ViewConfigs[ovrEye_Count];
	XrView DefaultEyeViews[ovrEye_Count];

	// Session status
	SessionStatusBits SessionStatus;
	ovrPosef CalibratedOrigin;

	// Hacks
	std::unique_ptr<SessionDetails> Details;

	// Input
	std::unique_ptr<InputManager> Input;

	ovrResult BeginSession(void* graphicsBinding);
	ovrResult EndSession();
};
