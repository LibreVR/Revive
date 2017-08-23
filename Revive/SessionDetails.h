#pragma once

#include <map>
#include <list>
#include <atomic>
#include <openvr.h>

#include "OVR_CAPI.h"

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
	};

	SessionDetails();
	~SessionDetails();

	bool UseHack(Hack hack);

	std::atomic<ovrHmdDesc*> HmdDesc;
	void UpdateHmdDesc();

	std::atomic_uint32_t TrackerCount;
	std::atomic<ovrTrackerDesc*> TrackerDesc[vr::k_unMaxTrackedDeviceCount];
	void UpdateTrackerDesc();

private:
	struct HackInfo
	{
		const char* m_filename; // The filename of the main executable
		Hack m_hack;            // Which hack is it?
		bool m_usehack;         // Should it use the hack?
	};

	static HackInfo m_known_hacks[];
	
	std::map<Hack, HackInfo> m_hacks;
	std::list<ovrHmdDesc> HmdDescList;
	std::list<ovrTrackerDesc> TrackerDescList;
};

