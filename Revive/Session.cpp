#include "Session.h"
#include "CompositorBase.h"
#include "InputManager.h"
#include "REV_Math.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <openvr.h>
#include <assert.h>
#include <map>

struct HackInfo
{
	const char* m_filename; // The filename of the main executable
	const char* m_driver;	// The name of the driver
	Hack m_hack;            // Which hack is it?
	bool m_usehack;         // Should it use the hack?
};

HackInfo g_known_hacks[] = {
	{ "Stormland.exe", nullptr, HACK_SAME_FOV_FOR_BOTH_EYES, true },
	{ "ultrawings.exe", nullptr, HACK_FAKE_PRODUCT_NAME, true },
	{ "AirMech.exe", nullptr, HACK_SLEEP_IN_SESSION_STATUS, true },
	{ nullptr, "lighthouse", HACK_SPOOF_SENSORS, false },
	{ "DCVR-Win64-Shipping.exe", nullptr, HACK_DISABLE_STATS, true }
};
std::map<Hack, HackInfo> g_hacks;

ovrHmdStruct::ovrHmdStruct()
	: Status()
	, StringBuffer()
	, TrackingOrigin(vr::TrackingUniverseSeated)
	, FrameIndex(0)
	, StatsIndex(0)
	, BaseStats()
	, Compositor(nullptr)
	, Input(new InputManager())
{
	// Oculus games expect a seated tracking space by default
	vr::VRCompositor()->SetTrackingSpace(TrackingOrigin);

	Status.HmdPresent = vr::VR_IsHmdPresent();
	Status.HmdMounted = true;
	Status.HasInputFocus = vr::VRSystem()->IsInputAvailable();
	Status.OverlayPresent = vr::VROverlay()->IsDashboardVisible();

	char filepath[MAX_PATH];
	GetModuleFileNameA(NULL, filepath, MAX_PATH);
	char* filename = PathFindFileNameA(filepath);

	uint32_t size = vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
		vr::Prop_TrackingSystemName_String, nullptr, 0);
	std::vector<char> driver(size);
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
		vr::Prop_TrackingSystemName_String, driver.data(), size);

	for (auto& hack : g_known_hacks)
	{
		if ((!hack.m_filename || _stricmp(filename, hack.m_filename) == 0) &&
			(!hack.m_driver || strcmp(driver.data(), hack.m_driver) == 0))
		{
			if (hack.m_usehack)
				g_hacks.emplace(hack.m_hack, hack);
		}
		else if (!hack.m_usehack)
		{
			g_hacks.emplace(hack.m_hack, hack);
		}
	}

	UpdateHmdDesc();
	UpdateTrackerDesc();
}

bool ovrHmdStruct::UseHack(Hack hack) const
{
	return g_hacks.find(hack) != g_hacks.end();
}

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
				UpdateTrackerDesc();
			else if (deviceClass == vr::TrackedDeviceClass_HMD)
				UpdateHmdDesc();
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

void ovrHmdStruct::UpdateHmdDesc()
{
	HmdDesc.Type = ovrHmd_CV1;

	// Get HMD name
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, HmdDesc.ProductName, 64);
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, HmdDesc.Manufacturer, 64);

	// Some games require a fake product name
	if (UseHack(HACK_FAKE_PRODUCT_NAME))
		strcpy_s(HmdDesc.ProductName, 64, "Oculus Rift");

	// TODO: Get HID information
	HmdDesc.VendorId = 0;
	HmdDesc.ProductId = 0;

	// Get serial number
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, HmdDesc.SerialNumber, 24);

	// TODO: Get firmware version
	HmdDesc.FirmwareMajor = 0;
	HmdDesc.FirmwareMinor = 0;

	// Get capabilities
	HmdDesc.AvailableHmdCaps = 0;
	HmdDesc.DefaultHmdCaps = 0;
	HmdDesc.AvailableTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_Position;
	if (!vr::VRSystem()->GetBoolTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_WillDriftInYaw_Bool))
		HmdDesc.AvailableTrackingCaps |= ovrTrackingCap_MagYawCorrection;
	HmdDesc.DefaultTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position;

	// Get the render target size
	ovrSizei size;
	vr::VRSystem()->GetRecommendedRenderTargetSize((uint32_t*)&size.w, (uint32_t*)&size.h);

	// Update the render descriptors
	for (int eye = 0; eye < ovrEye_Count; eye++)
	{
		ovrEyeRenderDesc& desc = RenderDesc[eye];

		OVR::FovPort eyeFov;
		vr::VRSystem()->GetProjectionRaw((vr::EVREye)eye, &eyeFov.LeftTan, &eyeFov.RightTan, &eyeFov.DownTan, &eyeFov.UpTan);
		eyeFov.LeftTan *= -1.0f;
		eyeFov.DownTan *= -1.0f;

		desc.Eye = (ovrEyeType)eye;
		desc.Fov = eyeFov;

		desc.DistortedViewport = OVR::Recti(eye == ovrEye_Left ? 0 : size.w, 0, size.w, size.h);
		desc.PixelsPerTanAngleAtCenter = OVR::Vector2f(size.w * (MATH_FLOAT_PIOVER4 / eyeFov.GetHorizontalFovRadians()),
			size.h * (MATH_FLOAT_PIOVER4 / eyeFov.GetVerticalFovRadians()));

		if (UseHack(HACK_RECONSTRUCT_EYE_MATRIX))
		{
			float ipd = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserIpdMeters_Float);
			desc.HmdToEyePose.Orientation = OVR::Quatf::Identity();
			desc.HmdToEyePose.Position.x = (eye == ovrEye_Left) ? -ipd / 2.0f : ipd / 2.0f;
		}
		else
		{
			REV::Matrix4f HmdToEyeMatrix = (REV::Matrix4f)vr::VRSystem()->GetEyeToHeadTransform((vr::EVREye)eye);
			desc.HmdToEyePose = OVR::Posef(OVR::Quatf(HmdToEyeMatrix), HmdToEyeMatrix.GetTranslation());
		}

		// Update the HMD descriptor
		HmdDesc.DefaultEyeFov[eye] = eyeFov;
		HmdDesc.MaxEyeFov[eye] = eyeFov;

		if (eye != 0 && UseHack(HACK_SAME_FOV_FOR_BOTH_EYES)) {
			HmdDesc.DefaultEyeFov[eye] = HmdDesc.DefaultEyeFov[0];
			std::swap(HmdDesc.DefaultEyeFov[eye].LeftTan, HmdDesc.DefaultEyeFov[eye].RightTan);
			HmdDesc.MaxEyeFov[eye] = HmdDesc.DefaultEyeFov[eye];
		}
	}

	// Get the display properties
	HmdDesc.Resolution = size;
	HmdDesc.Resolution.w *= 2; // Both eye ports
	HmdDesc.DisplayRefreshRate = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
}

void ovrHmdStruct::UpdateTrackerDesc()
{
	const bool spoofSensors = UseHack(HACK_SPOOF_SENSORS);
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount];
	uint32_t count = 3;

	if (!spoofSensors)
		count = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, trackers, vr::k_unMaxTrackedDeviceCount);

	// Only update trackers we haven't seen yet, this ensures we don't run into concurrency errors
	for (uint32_t i = TrackerCount; i < count; i++)
	{
		ovrTrackerDesc& desc = TrackerDesc[i];

		if (spoofSensors)
		{
			desc.FrustumHFovInRadians = OVR::DegreeToRad(100.0f);
			desc.FrustumVFovInRadians = OVR::DegreeToRad(70.0f);
			desc.FrustumNearZInMeters = 0.4f;
			desc.FrustumFarZInMeters = 2.5f;
		}
		else
		{
			vr::TrackedDeviceIndex_t index = trackers[i];

			// Calculate field-of-view.
			float left = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewLeftDegrees_Float);
			float right = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewRightDegrees_Float);
			float top = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewTopDegrees_Float);
			float bottom = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewBottomDegrees_Float);
			desc.FrustumHFovInRadians = (float)OVR::DegreeToRad(left + right);
			desc.FrustumVFovInRadians = (float)OVR::DegreeToRad(top + bottom);

			// Get the tracking frustum.
			desc.FrustumNearZInMeters = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_TrackingRangeMinimumMeters_Float);
			desc.FrustumFarZInMeters = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_TrackingRangeMaximumMeters_Float);
		}
	}

	// Update the tracker count, this also serves as a memory barrier for the array
	TrackerCount = count;
}
