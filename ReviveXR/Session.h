#pragma once

#include <OVR_CAPI.h>
#include <openxr/openxr.h>
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
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
	std::map<void**, void*> HookedFunctions;

	// Synchronization
	std::pair<std::mutex,
		std::condition_variable> Running;
	std::shared_mutex TrackingMutex;

	// System handles
	XrInstance Instance;
	XrSystemId System;
	XrSession Session;

	// Tracking handles
	XrSpace ViewSpace;
	std::atomic<ovrTrackingOrigin> TrackingOrigin;
	XrSpace OriginSpaces[ovrTrackingOrigin_Count];
	XrSpace	TrackingSpaces[ovrTrackingOrigin_Count];

	// Frame state
	XrIndexedFrameState FrameStats[ovrMaxProvidedFrameStats];
	std::atomic<XrIndexedFrameState*> CurrentFrame;
	ovrGraphicsLuid Adapter;

	// OpenXR properties
	XrSystemProperties SystemProperties;
	XrViewConfigurationView ViewConfigs[ovrEye_Count];
	XrViewConfigurationViewFovEPIC ViewFov[ovrEye_Count];
	XrView ViewPoses[ovrEye_Count];
	ovrVector2f PixelsPerTan[ovrEye_Count];
	std::vector<int64_t> SupportedFormats;

	// Session status
	std::atomic<SessionStatusBits> SessionStatus;

	// Field-of-view stencil
	std::map<XrVisibilityMaskTypeKHR, VisibilityMask> VisibilityMasks[ovrEye_Count];

	// Input
	std::unique_ptr<InputManager> Input;

	ovrResult InitSession(XrInstance instance);
	ovrResult BeginSession(void* graphicsBinding, bool beginFrame = true);
	ovrResult EndSession();

	ovrResult UpdateStencil(ovrEyeType view, XrVisibilityMaskTypeKHR type);
	ovrResult LocateViews(XrView out_Views[ovrEye_Count], XrViewStateFlags* out_Flags = nullptr) const;
	bool SupportsFormat(int64_t format) const;
};
