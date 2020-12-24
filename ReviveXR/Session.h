#pragma once

#include <OVR_CAPI.h>
#include <openxr/openxr.h>
#include <memory>
#include <utility>
#include <atomic>
#include <mutex>
#include <list>

class Extensions;
class RuntimeDetails;
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

	// Swapchain management
	std::mutex ChainMutex;
	std::list<XrSwapchain> AcquiredChains;

	// OpenXR properties
	XrSystemProperties SystemProperties;
	XrReferenceSpaceType TrackingSpace;
	XrViewConfigurationView ViewConfigs[ovrEye_Count];
	XrView DefaultEyeViews[ovrEye_Count];

	// Session status
	SessionStatusBits SessionStatus;
	ovrPosef CalibratedOrigin;

	// Runtime
	Extensions* Extensions;
	RuntimeDetails* Details;

	// Input
	std::unique_ptr<InputManager> Input;

	ovrResult BeginSession(void* graphicsBinding);
	ovrResult EndSession();
};
