#pragma once

#include <OVR_CAPI.h>
#include <openxr/openxr.h>
#include <memory>

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

struct ovrHmdStruct
{
	long long NextFrame;

	// OpenXR handles
	XrInstance Instance;
	XrSystemId System;
	XrSession Session;
	XrSpace ViewSpace;
	XrSpace	LocalSpace;
	XrSpace StageSpace;
	XrFrameState FrameState;

	// OpenXR properties
	XrSystemProperties SystemProperties;
	XrReferenceSpaceType TrackingSpace;
	XrViewConfigurationView ViewConfigs[ovrEye_Count];
	XrFovf DefaultEyeFov[ovrEye_Count];

	// Session status
	SessionStatusBits SessionStatus;

	std::unique_ptr<InputManager> Input;
};
