#pragma once

#include <map>
#include <atomic>
#include <openvr.h>

#include "OVR_CAPI.h"
#include "rcu_ptr.h"

class SessionDetails
{
public:
	enum Hack
	{
		// Hack: Wait for running start in ovr_GetTrackingState().
		// Games like Dirt Rally do a lot of rendering-independent work on the rendering thread.
		// Calling WaitGetPoses() in ovr_GetTrackingState() allows Dirt Rally to do that work before
		// we block waiting for running state.
		HACK_WAIT_IN_TRACKING_STATE,

		// Hack: Use a fake product name.
		// Games like Ultrawings will actually check whether the product name contains the string
		// "Oculus", so we use a fake name for the HMD to work around this issue.
		HACK_FAKE_PRODUCT_NAME,

		// Hack: Spoof the number of connected sensors.
		// Some headsets don't have external trackers, so we have to spoof the number of connected
		// sensors.
		HACK_SPOOF_SENSORS,

		// Hack: Calculate the eye matrix based on the IPD.
		// Some driver don't properly report the eye matrix, but do correctly report the IPD.
		// We can use the IPD to reconstruct the eye matrix.
		HACK_RECONSTRUCT_EYE_MATRIX,

		// Hack: Insert a small sleep duration in ovr_GetSessionStatus().
		// AirMech: Command doesn't properly synchronize their threads and relies on actual API call
		// timings to keep the game thread in sync with the render thread.
		HACK_SLEEP_IN_SESSION_STATUS,

		// Hack: Only uses poses from the compositor for rendering.
		// Some driver don't support pose submission therefore we can't use predicted poses for rendering
		// that did not come from the compositor.
		HACK_STRICT_POSES,

		// Hack: Disable support for performance statistics.
		// Dance Central VR crashes when any of the compositor statistics calls are made.
		HACK_DISABLE_STATS,

		// Hack: Wait as soon as the submit is done.
		// Some apps like Dance Central VR don't handle waiting in the game thread very well.
		HACK_WAIT_ON_SUBMIT,

		// Hack: Use the same (mirrored) FOV for both eyes.
		// Stormland renders certain effects at the wrong depth if the eye FOVs do not match.
		HACK_SAME_FOV_FOR_BOTH_EYES,
	};

	SessionDetails();
	~SessionDetails();

	bool UseHack(Hack hack);

	rcu_ptr<ovrHmdDesc> HmdDesc;
	rcu_ptr<ovrEyeRenderDesc> RenderDesc[ovrEye_Count];
	void UpdateHmdDesc();

	std::atomic_uint32_t TrackerCount;
	rcu_ptr<ovrTrackerDesc> TrackerDesc[vr::k_unMaxTrackedDeviceCount];
	void UpdateTrackerDesc();

private:
	struct HackInfo
	{
		const char* m_filename; // The filename of the main executable
		const char* m_driver;	// The name of the driver
		Hack m_hack;            // Which hack is it?
		bool m_usehack;         // Should it use the hack?
	};

	static HackInfo m_known_hacks[];
	std::map<Hack, HackInfo> m_hacks;
};
