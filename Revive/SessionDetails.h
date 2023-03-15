#pragma once

#include <map>
#include <atomic>
#include <openvr.h>

#include "OVR_CAPI.h"

class SessionDetails
{
public:
	enum Hack
	{
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

		// Hack: Disable support for performance statistics.
		// Dance Central VR crashes when any of the compositor statistics calls are made.
		HACK_DISABLE_STATS,

		// Hack: Use the same (mirrored) FOV for both eyes.
		// Stormland renders certain effects at the wrong depth if the eye FOVs do not match.
		HACK_SAME_FOV_FOR_BOTH_EYES,
	};

	SessionDetails();
	~SessionDetails();

	bool UseHack(Hack hack);

	std::atomic_uint32_t TrackerCount;
	void UpdateTrackerDesc();

	const ovrHmdDesc* GetHmdDesc() const { return &HmdDesc; }
	const ovrEyeRenderDesc* GetRenderDesc(ovrEyeType eye) const { return &RenderDesc[eye]; }
	const ovrTrackerDesc* GetTrackerDesc(unsigned int index) const
	{
		return index < vr::k_unMaxTrackedDeviceCount ? &TrackerDesc[index] : nullptr;
	}

	float GetRefreshRate() const { return HmdDesc.DisplayRefreshRate; }
	float GetVsyncToPhotons() const { return fVsyncToPhotons; }

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

	float fVsyncToPhotons;
	ovrHmdDesc HmdDesc;
	ovrEyeRenderDesc RenderDesc[ovrEye_Count];
	ovrTrackerDesc TrackerDesc[vr::k_unMaxTrackedDeviceCount];

	void UpdateHmdDesc();
};
