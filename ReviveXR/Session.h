#pragma once

#include <OVR_CAPI.h>
#include <openxr/openxr.h>
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <list>
#include <vector>
#include <utility>

class Runtime;
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

typedef std::pair<std::vector<XrVector2f>,
	std::vector<uint32_t>> VisibilityMask;

struct ovrHmdStruct
{
	std::pair<std::mutex,
		std::condition_variable> Running;

	std::map<void**, void*> HookedFunctions;

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
	ovrGraphicsLuid Adapter;

	// OpenXR properties
	XrSystemProperties SystemProperties;
	XrReferenceSpaceType TrackingSpace;
	XrViewConfigurationView ViewConfigs[ovrEye_Count];
	XrViewConfigurationViewFovEPIC ViewFov[ovrEye_Count];
	XrView ViewPoses[ovrEye_Count];
	ovrVector2f PixelsPerTan[ovrEye_Count];

	// Session status
	SessionStatusBits SessionStatus;
	ovrPosef CalibratedOrigin;

	// Field-of-view stencil
	std::map<XrVisibilityMaskTypeKHR, VisibilityMask> VisibilityMasks[ovrEye_Count];

	// Input
	std::unique_ptr<InputManager> Input;

	ovrResult InitSession(XrInstance instance);
	ovrResult BeginSession(void* graphicsBinding, bool beginFrame = true);
	ovrResult EndSession();

	ovrResult UpdateStencil(ovrEyeType view, XrVisibilityMaskTypeKHR type);
	ovrResult LocateViews(XrView out_Views[ovrEye_Count], XrViewStateFlags* out_Flags = nullptr) const;
};
