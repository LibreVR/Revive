#pragma once

#include <OVR_CAPI.h>
#include <openvr.h>
#include <memory>
#include <atomic>
#include <list>
#include <thread>

// Forward declarations
class CompositorBase;
class InputManager;
class SessionDetails;
class ProfileManager;

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
	uint32_t MinorVersion;

	// Session thread
	std::thread SessionThread;
	std::atomic_bool Running;

	// Session status
	std::atomic<SessionStatusBits> SessionStatus;
	char StringBuffer[vr::k_unMaxPropertyStringSize];
	vr::ETrackingUniverseOrigin TrackingOrigin;

	// Compositor statistics
	std::atomic_llong FrameIndex;
	long long StatsIndex;
	vr::Compositor_CumulativeStats BaseStats;

	// Revive interfaces
	std::unique_ptr<CompositorBase> Compositor;
	std::unique_ptr<InputManager> Input;
	std::unique_ptr<SessionDetails> Details;

	ovrHmdStruct();
	~ovrHmdStruct();
};
