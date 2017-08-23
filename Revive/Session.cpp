#include "Session.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"
#include "SettingsManager.h"
#include "Settings.h"

void SessionThreadFunc(ovrSession session)
{
	std::chrono::microseconds freq(std::chrono::milliseconds(10));

	while (session->Running)
	{
		vr::VREvent_t vrEvent;
		while (vr::VRSystem()->PollNextEvent(&vrEvent, sizeof(vrEvent)))
		{
			switch (vrEvent.eventType)
			{
			case vr::VREvent_TrackedDeviceActivated:
			case vr::VREvent_TrackedDeviceRoleChanged:
			case vr::VREvent_TrackedDeviceDeactivated:
			{
				vr::ETrackedDeviceClass deviceClass = vr::VRSystem()->GetTrackedDeviceClass(vrEvent.trackedDeviceIndex);
				if (deviceClass == vr::TrackedDeviceClass_Controller)
					session->Input->UpdateConnectedControllers();
				else if (deviceClass == vr::TrackedDeviceClass_HMD)
					session->Details->UpdateHmdDesc();
				else if (deviceClass == vr::TrackedDeviceClass_TrackingReference)
					session->Details->UpdateTrackerDesc();
			}
			break;
			}
		}

		std::this_thread::sleep_for(freq);
	}
}

ovrHmdStruct::ovrHmdStruct()
	: Running(true)
	, ShouldQuit(false)
	, IsVisible(false)
	, FrameIndex(0)
	, StatsIndex(0)
	, Compositor(nullptr)
	, Input(new InputManager())
	, Details(new SessionDetails())
	, Settings(new SettingsManager())
{
	memset(StringBuffer, 0, sizeof(StringBuffer));
	memset(&ResetStats, 0, sizeof(ResetStats));
	memset(Stats, 0, sizeof(Stats));

	// Get the default universe origin from the settings
	TrackingOrigin = (vr::ETrackingUniverseOrigin)ovr_GetInt(this, REV_KEY_DEFAULT_ORIGIN, REV_DEFAULT_ORIGIN);

	SessionThread = std::thread(SessionThreadFunc, this);
}

ovrHmdStruct::~ovrHmdStruct()
{
	Running = false;
	if (SessionThread.joinable())
		SessionThread.join();
}
