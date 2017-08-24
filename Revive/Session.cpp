#include "Session.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"
#include "SettingsManager.h"
#include "Settings.h"

void SessionThreadFunc(ovrSession session)
{
	std::chrono::microseconds freq(std::chrono::milliseconds(10));
	DWORD procId = GetCurrentProcessId();

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
			case vr::VREvent_SceneApplicationChanged:
			{
				SessionStatusBits status = session->SessionStatus;
				status.IsVisible = vrEvent.data.process.pid == procId;
				session->SessionStatus = status;
			}
			break;
			case vr::VREvent_Quit:
			{
				SessionStatusBits status = session->SessionStatus;
				status.ShouldQuit = true;
				session->SessionStatus = status;
				vr::VRSystem()->AcknowledgeQuit_Exiting();
			}
			break;
			case vr::VREvent_TrackedDeviceUserInteractionStarted:
			case vr::VREvent_TrackedDeviceUserInteractionEnded:
			if (vrEvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd)
			{
				SessionStatusBits status = session->SessionStatus;
				status.HmdMounted = vrEvent.eventType == vr::VREvent_TrackedDeviceUserInteractionStarted;
				session->SessionStatus = status;
			}
			break;
			}

#ifdef DEBUG
			OutputDebugStringA(vr::VRSystem()->GetEventTypeNameFromEnum((vr::EVREventType)vrEvent.eventType));
			OutputDebugStringA("\n");
#endif
		}

		std::this_thread::sleep_for(freq);
	}
}

ovrHmdStruct::ovrHmdStruct()
	: Running(true)
	, SessionStatus()
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

	SessionStatusBits status = {};
	status.HmdPresent = vr::VR_IsHmdPresent();
	status.HmdMounted = status.HmdPresent;
	SessionStatus = status;

	SessionThread = std::thread(SessionThreadFunc, this);
}

ovrHmdStruct::~ovrHmdStruct()
{
	Running = false;
	if (SessionThread.joinable())
		SessionThread.join();
}
