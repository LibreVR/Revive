#include "SessionDetails.h"
#include "REV_Math.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <openvr.h>
#include <vector>
#include <memory>

SessionDetails::HackInfo SessionDetails::m_known_hacks[] = {
	{ "Stormland.exe", nullptr, HACK_SAME_FOV_FOR_BOTH_EYES, true },
	{ "ultrawings.exe", nullptr, HACK_FAKE_PRODUCT_NAME, true },
	{ "AirMech.exe", nullptr, HACK_SLEEP_IN_SESSION_STATUS, true },
	{ nullptr, "lighthouse", HACK_SPOOF_SENSORS, false },
	{ nullptr, "lighthouse", HACK_STRICT_POSES, false },
	{ "DCVR-Win64-Shipping.exe", nullptr, HACK_DISABLE_STATS, true }
};

SessionDetails::SessionDetails()
	: HmdDesc()
	, TrackerDesc()
	, TrackerCount(0)
{
	char filepath[MAX_PATH];
	GetModuleFileNameA(NULL, filepath, MAX_PATH);
	char* filename = PathFindFileNameA(filepath);

	uint32_t size = vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
		vr::Prop_TrackingSystemName_String, nullptr, 0);
	std::vector<char> driver(size);
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
		vr::Prop_TrackingSystemName_String, driver.data(), size);

	for (auto& hack : m_known_hacks)
	{
		if ((!hack.m_filename || _stricmp(filename, hack.m_filename) == 0) &&
			(!hack.m_driver || strcmp(driver.data(), hack.m_driver) == 0))
		{
			if (hack.m_usehack)
				m_hacks.emplace(hack.m_hack, hack);
		}
		else if (!hack.m_usehack)
		{
			m_hacks.emplace(hack.m_hack, hack);
		}
	}

	UpdateHmdDesc();
	UpdateTrackerDesc();
}

SessionDetails::~SessionDetails()
{
}

bool SessionDetails::UseHack(Hack hack)
{
	return m_hacks.find(hack) != m_hacks.end();
}

void SessionDetails::UpdateHmdDesc()
{
	std::shared_ptr<ovrHmdDesc> desc = std::make_shared<ovrHmdDesc>();

	desc->Type = ovrHmd_CV1;

	// Get HMD name
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, desc->ProductName, 64);
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, desc->Manufacturer, 64);

	// Some games require a fake product name
	if (UseHack(SessionDetails::HACK_FAKE_PRODUCT_NAME))
		strcpy_s(desc->ProductName, 64, "Oculus Rift");

	// TODO: Get HID information
	desc->VendorId = 0;
	desc->ProductId = 0;

	// Get serial number
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, desc->SerialNumber, 24);

	// TODO: Get firmware version
	desc->FirmwareMajor = 0;
	desc->FirmwareMinor = 0;

	// Get capabilities
	desc->AvailableHmdCaps = 0;
	desc->DefaultHmdCaps = 0;
	desc->AvailableTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_Position;
	if (!vr::VRSystem()->GetBoolTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_WillDriftInYaw_Bool))
		desc->AvailableTrackingCaps |= ovrTrackingCap_MagYawCorrection;
	desc->DefaultTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position;

	// Get the render target size
	ovrSizei size;
	vr::VRSystem()->GetRecommendedRenderTargetSize((uint32_t*)&size.w, (uint32_t*)&size.h);

	// Update the render descriptors
	for (int eye = 0; eye < ovrEye_Count; eye++)
	{
		std::shared_ptr<ovrEyeRenderDesc> eyeDesc = std::make_shared<ovrEyeRenderDesc>();

		OVR::FovPort eyeFov;
		vr::VRSystem()->GetProjectionRaw((vr::EVREye)eye, &eyeFov.LeftTan, &eyeFov.RightTan, &eyeFov.DownTan, &eyeFov.UpTan);
		eyeFov.LeftTan *= -1.0f;
		eyeFov.DownTan *= -1.0f;

		eyeDesc->Eye = (ovrEyeType)eye;
		eyeDesc->Fov = eyeFov;

		eyeDesc->DistortedViewport = OVR::Recti(eye == ovrEye_Left ? 0 : size.w, 0, size.w, size.h);
		eyeDesc->PixelsPerTanAngleAtCenter = OVR::Vector2f(size.w * (MATH_FLOAT_PIOVER4 / eyeFov.GetHorizontalFovRadians()),
			size.h * (MATH_FLOAT_PIOVER4 / eyeFov.GetVerticalFovRadians()));

		if (UseHack(HACK_RECONSTRUCT_EYE_MATRIX))
		{
			float ipd = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserIpdMeters_Float);
			eyeDesc->HmdToEyePose.Orientation = OVR::Quatf::Identity();
			eyeDesc->HmdToEyePose.Position.x = (eye == ovrEye_Left) ? -ipd / 2.0f : ipd / 2.0f;
		}
		else
		{
			REV::Matrix4f HmdToEyeMatrix = (REV::Matrix4f)vr::VRSystem()->GetEyeToHeadTransform((vr::EVREye)eye);
			eyeDesc->HmdToEyePose = OVR::Posef(OVR::Quatf(HmdToEyeMatrix), HmdToEyeMatrix.GetTranslation());
		}

		// Swap in the new copy
		RenderDesc[eye].swap(eyeDesc);

		// Update the HMD descriptor
		desc->DefaultEyeFov[eye] = eyeFov;
		desc->MaxEyeFov[eye] = eyeFov;

		if (eye != 0 && UseHack(HACK_SAME_FOV_FOR_BOTH_EYES)) {
			desc->DefaultEyeFov[eye] = desc->DefaultEyeFov[0];
			std::swap(desc->DefaultEyeFov[eye].LeftTan, desc->DefaultEyeFov[eye].RightTan);
			desc->MaxEyeFov[eye] = desc->DefaultEyeFov[eye];
		}
	}

	// Get the display properties
	desc->Resolution = size;
	desc->Resolution.w *= 2; // Both eye ports
	desc->DisplayRefreshRate = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

	// Swap in the new copy
	HmdDesc.swap(desc);
}

void SessionDetails::UpdateTrackerDesc()
{
	const bool spoofSensors = UseHack(SessionDetails::HACK_SPOOF_SENSORS);
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount];
	uint32_t count = 3;

	if (!spoofSensors)
		count = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, trackers, vr::k_unMaxTrackedDeviceCount);

	for (uint32_t i = 0; i < count; i++)
	{
		// Fill the descriptor.
		std::shared_ptr<ovrTrackerDesc> desc = std::make_shared<ovrTrackerDesc>();

		if (spoofSensors)
		{
			desc->FrustumHFovInRadians = OVR::DegreeToRad(100.0f);
			desc->FrustumVFovInRadians = OVR::DegreeToRad(70.0f);
			desc->FrustumNearZInMeters = 0.4f;
			desc->FrustumFarZInMeters = 2.5f;
		}
		else 
		{
			vr::TrackedDeviceIndex_t index = trackers[i];

			// Calculate field-of-view.
			float left = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewLeftDegrees_Float);
			float right = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewRightDegrees_Float);
			float top = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewTopDegrees_Float);
			float bottom = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewBottomDegrees_Float);
			desc->FrustumHFovInRadians = (float)OVR::DegreeToRad(left + right);
			desc->FrustumVFovInRadians = (float)OVR::DegreeToRad(top + bottom);

			// Get the tracking frustum.
			desc->FrustumNearZInMeters = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_TrackingRangeMinimumMeters_Float);
			desc->FrustumFarZInMeters = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_TrackingRangeMaximumMeters_Float);
		}

		// Swap in the new copy
		TrackerDesc[i].swap(desc);
	}

	// Update the tracker count, this also serves as a memory barrier for the array
	TrackerCount = count;
}
