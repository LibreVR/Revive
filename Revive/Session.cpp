#include "Session.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"

#include <Windows.h>
#include <assert.h>

void ovrHmdStruct::UpdateStatus()
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
				Input->UpdateConnectedControllers();
			else if (deviceClass == vr::TrackedDeviceClass_TrackingReference)
				Details->UpdateTrackerDesc();
			assert(deviceClass != vr::TrackedDeviceClass_HMD);
		}
		break;
		case vr::VREvent_TrackedDeviceRoleChanged:
		{
			Input->UpdateConnectedControllers();
		}
		break;
		case vr::VREvent_SceneApplicationChanged:
		{
			Status.IsVisible = vrEvent.data.process.pid == GetCurrentProcessId();
		}
		break;
		case vr::VREvent_Quit:
		{
			Status.ShouldQuit = true;
			vr::VRSystem()->AcknowledgeQuit_Exiting();
		}
		break;
		case vr::VREvent_TrackedDeviceUserInteractionStarted:
		case vr::VREvent_TrackedDeviceUserInteractionEnded:
		if (vrEvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd)
		{
			Status.HmdMounted = vrEvent.eventType == vr::VREvent_TrackedDeviceUserInteractionStarted;
		}
		break;
		case vr::VREvent_InputFocusChanged:
		{
			Status.HasInputFocus = vr::VRSystem()->IsInputAvailable();
		}
		break;
		case vr::VREvent_DashboardActivated:
		case vr::VREvent_DashboardDeactivated:
		{
			Status.OverlayPresent = vr::VROverlay()->IsDashboardVisible();
		}
		break;
		}

#ifdef DEBUG
		OutputDebugStringA(vr::VRSystem()->GetEventTypeNameFromEnum((vr::EVREventType)vrEvent.eventType));
		OutputDebugStringA("\n");
#endif
	}
}

ovrHmdStruct::ovrHmdStruct()
	: Status()
	, StringBuffer()
	, TrackingOrigin(vr::TrackingUniverseSeated)
	, FrameIndex(0)
	, StatsIndex(0)
	, BaseStats()
	, Compositor(nullptr)
	, Input(new InputManager())
	, Details(new SessionDetails())
{
	// Oculus games expect a seated tracking space by default
	if (Details->UseHack(SessionDetails::HACK_STRICT_POSES))
		vr::VRCompositor()->SetTrackingSpace(TrackingOrigin);

	Status.HmdPresent = vr::VR_IsHmdPresent();
	Status.HmdMounted = true;
	Status.HasInputFocus = vr::VRSystem()->IsInputAvailable();
	Status.OverlayPresent = vr::VROverlay()->IsDashboardVisible();
}
