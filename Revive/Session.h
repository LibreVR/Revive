#pragma once

#include <OVR_CAPI.h>
#include <openvr.h>
#include <memory>

// Forward declarations
enum revGripType;
class CompositorBase;
class InputManager;
class SessionDetails;

struct ovrHmdStruct
{
	// Session status
	bool ShouldQuit;
	bool IsVisible;
	char StringBuffer[vr::k_unMaxPropertyStringSize];

	// Compositor statistics
	long long FrameIndex;
	long long StatsIndex;
	ovrPerfStatsPerCompositorFrame ResetStats;
	vr::Compositor_CumulativeStats Stats[ovrMaxProvidedFrameStats];

	// Revive interfaces
	std::unique_ptr<CompositorBase> Compositor;
	std::unique_ptr<InputManager> Input;
	std::unique_ptr<SessionDetails> Details;

	// Revive settings
	float PixelsPerDisplayPixel;
	float Deadzone;
	float Sensitivity;
	revGripType ToggleGrip;
	float ToggleDelay;
	bool IgnoreActivity;
	ovrVector3f RotationOffset, PositionOffset;
	vr::HmdMatrix34_t TouchOffset[ovrHand_Count];

	ovrHmdStruct();
	void LoadSettings();
};
