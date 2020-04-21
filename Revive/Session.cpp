#include "Session.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"

#include <Windows.h>
#include <assert.h>

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
			case vr::VREvent_TrackedDeviceDeactivated:
			{
				vr::ETrackedDeviceClass deviceClass = vr::VRSystem()->GetTrackedDeviceClass(vrEvent.trackedDeviceIndex);
				if (deviceClass == vr::TrackedDeviceClass_Controller)
					session->Input->UpdateConnectedControllers();
				else if (deviceClass == vr::TrackedDeviceClass_TrackingReference)
					session->Details->UpdateTrackerDesc();
				assert(deviceClass != vr::TrackedDeviceClass_HMD);
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
			case vr::VREvent_InputFocusChanged:
			{
				SessionStatusBits status = session->SessionStatus;
				status.HasInputFocus = vr::VRSystem()->IsInputAvailable();
				session->SessionStatus = status;
			}
			break;
			case vr::VREvent_DashboardActivated:
			case vr::VREvent_DashboardDeactivated:
			{
				SessionStatusBits status = session->SessionStatus;
				status.OverlayPresent = vr::VROverlay()->IsDashboardVisible();
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

ovrHmdStruct::ovrHmdStruct(ProfileManager* profileManager)
	: Running(true)
	, SessionStatus()
	, StringBuffer()
	, TrackingOrigin(vr::TrackingUniverseSeated)
	, FrameIndex(0)
	, StatsIndex(0)
	, BaseStats()
	, Compositor(nullptr)
	, Input(new InputManager())
	, Details(new SessionDetails())
	, Profiler(profileManager)
{
	// Oculus games expect a seated tracking space by default
	if (Details->UseHack(SessionDetails::HACK_STRICT_POSES))
		vr::VRCompositor()->SetTrackingSpace(TrackingOrigin);

	SessionStatusBits status = {};
	status.HmdPresent = vr::VR_IsHmdPresent();
	status.HmdMounted = true;
	status.HasInputFocus = vr::VRSystem()->IsInputAvailable();
	status.OverlayPresent = vr::VROverlay()->IsDashboardVisible();
	SessionStatus = status;

	SessionThread = std::thread(SessionThreadFunc, this);
}

ovrHmdStruct::~ovrHmdStruct()
{
	Running = false;
	if (SessionThread.joinable())
		SessionThread.join();
}
