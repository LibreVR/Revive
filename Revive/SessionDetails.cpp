#include "SessionDetails.h"
#include "REV_Math.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <openvr.h>
#include <vector>

SessionDetails::HackInfo SessionDetails::m_known_hacks[] = {
	{ "drt.exe", nullptr, HACK_WAIT_IN_TRACKING_STATE, false }, // TODO: Fix this hack
	{ "ultrawings.exe", nullptr, HACK_FAKE_PRODUCT_NAME, true },
	{ nullptr, "holographic", HACK_SPOOF_SENSORS, true }
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
			m_hacks.emplace(hack.m_hack, hack);
	}

	UpdateHmdDesc();
	UpdateTrackerDesc();
}

SessionDetails::~SessionDetails()
{
}

bool SessionDetails::UseHack(Hack hack)
{
	auto it = m_hacks.find(hack);
	if (it == m_hacks.end())
		return false;
	return it->second.m_usehack;
}

void SessionDetails::UpdateHmdDesc()
{
	ovrHmdDesc desc = {};

	desc.Type = ovrHmd_CV1;

	// Get HMD name
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, desc.ProductName, 64);
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, desc.Manufacturer, 64);

	// Some games require a fake product name
	if (UseHack(SessionDetails::HACK_FAKE_PRODUCT_NAME))
		strncpy(desc.ProductName, "Oculus Rift", 64);

	// TODO: Get HID information
	desc.VendorId = 0;
	desc.ProductId = 0;

	// Get serial number
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, desc.SerialNumber, 24);

	// TODO: Get firmware version
	desc.FirmwareMajor = 0;
	desc.FirmwareMinor = 0;

	// Get capabilities
	desc.AvailableHmdCaps = 0;
	desc.DefaultHmdCaps = 0;
	desc.AvailableTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_Position;
	if (!vr::VRSystem()->GetBoolTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_WillDriftInYaw_Bool))
		desc.AvailableTrackingCaps |= ovrTrackingCap_MagYawCorrection;
	desc.DefaultTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position;

	// Get the render target size
	ovrSizei size;
	vr::VRSystem()->GetRecommendedRenderTargetSize((uint32_t*)&size.w, (uint32_t*)&size.h);

	// Update the render descriptors
	for (int i = 0; i < ovrEye_Count; i++)
	{
		ovrEyeRenderDesc eyeDesc = {};
		ovrFovPort eyeFov = {};

		vr::VRSystem()->GetProjectionRaw((vr::EVREye)i, &eyeFov.LeftTan, &eyeFov.RightTan, &eyeFov.DownTan, &eyeFov.UpTan);
		eyeFov.LeftTan *= -1.0f;
		eyeFov.DownTan *= -1.0f;

		eyeDesc.Eye = (ovrEyeType)i;
		eyeDesc.Fov = eyeFov;

		REV::Matrix4f HmdToEyeMatrix = (REV::Matrix4f)vr::VRSystem()->GetEyeToHeadTransform((vr::EVREye)i);
		float WidthTan = eyeFov.LeftTan + eyeFov.RightTan;
		float HeightTan = eyeFov.UpTan + eyeFov.DownTan;
		eyeDesc.DistortedViewport = OVR::Recti(i == ovrEye_Right ? size.w : 0, 0, size.w, size.h);
		eyeDesc.PixelsPerTanAngleAtCenter = OVR::Vector2f(size.w / WidthTan, size.h / HeightTan);
		eyeDesc.HmdToEyePose = OVR::Posef(OVR::Quatf(HmdToEyeMatrix), HmdToEyeMatrix.GetTranslation());

		// Add the state to the list and update the pointer
		RenderDescList.push_back(eyeDesc);
		RenderDesc[i] = &RenderDescList.back();

		// Update the HMD descriptor
		desc.DefaultEyeFov[i] = eyeFov;
		desc.MaxEyeFov[i] = eyeFov;
	}

	// Get the display properties
	desc.Resolution = size;
	desc.Resolution.w *= 2; // Both eye ports
	desc.DisplayRefreshRate = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

	// Add the state to the list and update the pointer
	HmdDescList.push_back(desc);
	HmdDesc = &HmdDescList.back();
}

void SessionDetails::UpdateTrackerDesc()
{
	// Get the index for this tracker.
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount];
	uint32_t count = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, trackers, vr::k_unMaxTrackedDeviceCount);

	for (uint32_t i = 0; i < count; i++)
	{
		vr::TrackedDeviceIndex_t index = trackers[i];

		// Fill the descriptor.
		ovrTrackerDesc desc;

		// Calculate field-of-view.
		float left = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewLeftDegrees_Float);
		float right = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewRightDegrees_Float);
		float top = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewTopDegrees_Float);
		float bottom = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewBottomDegrees_Float);
		desc.FrustumHFovInRadians = OVR::DegreeToRad(left + right);
		desc.FrustumVFovInRadians = OVR::DegreeToRad(top + bottom);

		// Get the tracking frustum.
		desc.FrustumNearZInMeters = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_TrackingRangeMinimumMeters_Float);
		desc.FrustumFarZInMeters = vr::VRSystem()->GetFloatTrackedDeviceProperty(index, vr::Prop_TrackingRangeMaximumMeters_Float);

		// Add the state to the list and update the pointer
		TrackerDescList.push_back(desc);
		TrackerDesc[i] = &TrackerDescList.back();
	}

	// Update the tracker count, this also serves as a memory barrier
	TrackerCount = count;
}
