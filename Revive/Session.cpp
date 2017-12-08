#include "Session.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"
#include "SettingsManager.h"
#include "Settings.h"

#define SESSION_RELOAD_EVENT_NAME L"ReviveReloadSettings"

void SessionThreadFunc(ovrSession session)
{
	std::chrono::microseconds freq(std::chrono::milliseconds(10));
	DWORD procId = GetCurrentProcessId();
	HANDLE reload = OpenEventW(SYNCHRONIZE, FALSE, SESSION_RELOAD_EVENT_NAME);

	while (session->Running)
	{
		vr::VREvent_t vrEvent;
		while (vr::VRSystem()->PollNextEvent(&vrEvent, sizeof(vrEvent)))
		{
			switch (vrEvent.eventType)
			{
			case vr::VREvent_TrackedDeviceActivated:
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
			case vr::VREvent_TrackedDeviceRoleChanged:
				session->Input->UpdateConnectedControllers();
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
			case vr::VREvent_InputFocusCaptured:
			case vr::VREvent_InputFocusReleased:
			{
				SessionStatusBits status = session->SessionStatus;
				status.HasInputFocus = vrEvent.eventType == vr::VREvent_InputFocusReleased;
				session->SessionStatus = status;
			}
			break;
			case vr::VREvent_DashboardActivated:
			case vr::VREvent_DashboardDeactivated:
			{
				SessionStatusBits status = session->SessionStatus;
				status.OverlayPresent = vrEvent.eventType == vr::VREvent_DashboardActivated;
				session->SessionStatus = status;
			}
			break;
			}

#ifdef DEBUG
			OutputDebugStringA(vr::VRSystem()->GetEventTypeNameFromEnum((vr::EVREventType)vrEvent.eventType));
			OutputDebugStringA("\n");
#endif
		}

		if (reload && WaitForSingleObject(reload, 0) == WAIT_OBJECT_0)
		{
			session->Settings->ReloadSettings();
			session->Input->LoadInputScript(session->Settings->InputScript.c_str());
		}

		std::this_thread::sleep_for(freq);
	}
}

ovrHmdStruct::ovrHmdStruct()
	: Running(true)
	, SessionStatus()
	, StringBuffer()
	, FrameIndex(0)
	, StatsIndex(0)
	, BaseStats()
	, Compositor(nullptr)
	, Input(new InputManager())
	, Details(new SessionDetails())
	, Settings(new SettingsManager())
{
	// Get the default universe origin from the settings
	TrackingOrigin = (vr::ETrackingUniverseOrigin)Settings->Get<int>(REV_KEY_DEFAULT_ORIGIN, REV_DEFAULT_ORIGIN);

	SessionStatusBits status = {};
	status.HmdPresent = vr::VR_IsHmdPresent();
	vr::EDeviceActivityLevel activity = vr::VRSystem()->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd);
	status.HmdMounted = activity != vr::k_EDeviceActivityLevel_Idle;
	status.HasInputFocus = !vr::VRSystem()->IsInputFocusCapturedByAnotherProcess();
	status.OverlayPresent = vr::VROverlay()->IsDashboardVisible();
	SessionStatus = status;

	Input->LoadInputScript(Settings->InputScript.c_str());
	SessionThread = std::thread(SessionThreadFunc, this);
}

ovrHmdStruct::~ovrHmdStruct()
{
	Running = false;
	if (SessionThread.joinable())
		SessionThread.join();
}
