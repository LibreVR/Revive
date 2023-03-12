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

struct ovrHmdStruct
{
	uint32_t MinorVersion;

	// Session status
	ovrSessionStatus Status;
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

	void UpdateStatus();
};
