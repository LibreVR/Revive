#include "OVR_CAPI.h"
#include "OVR_Version.h"
#include "Extras/OVR_Math.h"

#include "openvr.h"
#include <DXGI.h>
#include <Xinput.h>

#include "REV_Assert.h"
#include "REV_Error.h"
#include "REV_Math.h"

vr::EVRInitError g_InitError = vr::VRInitError_None;
vr::IVRSystem* g_VRSystem = nullptr;
IDXGIFactory* g_pFactory = nullptr;

struct ovrHmdStruct
{
	vr::IVRCompositor* compositor;
};

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params)
{
	g_VRSystem = vr::VR_Init(&g_InitError, vr::VRApplication_Scene);

	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&g_pFactory));
	if (FAILED(hr))
		return ovrError_IncompatibleGPU;

	return REV_InitErrorToOvrError(g_InitError);
}

OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{
	g_pFactory->Release();
	vr::VR_Shutdown();
}

OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo)
{
	VR_GetVRInitErrorAsEnglishDescription(g_InitError);
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString()
{
	return OVR_VERSION_STRING;
}

OVR_PUBLIC_FUNCTION(int) ovr_TraceMessage(int level, const char* message) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrHmdDesc) ovr_GetHmdDesc(ovrSession session)
{
	ovrHmdDesc desc;
	desc.Type = ovrHmd_CV1;

	// Get HMD name
	g_VRSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, desc.ProductName, 64);
	g_VRSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, desc.Manufacturer, 64);

	// Get EDID information
	desc.VendorId = (short)g_VRSystem->GetInt32TrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_EdidVendorID_Int32);
	desc.ProductId = (short)g_VRSystem->GetInt32TrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_EdidProductID_Int32);

	// Get serial number
	g_VRSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, desc.SerialNumber, 24);

	// Get firmware version
	desc.FirmwareMajor = (short)g_VRSystem->GetUint64TrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_FirmwareVersion_Uint64);
	desc.FirmwareMinor = 0;

	// Get capabilities
	desc.AvailableHmdCaps = 0;
	desc.DefaultHmdCaps = 0;
	desc.AvailableTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_Position;
	if (!g_VRSystem->GetBoolTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_WillDriftInYaw_Bool))
		desc.AvailableTrackingCaps |= ovrTrackingCap_MagYawCorrection;
	desc.DefaultTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position;

	// Get field-of-view
	for (int i = 0; i < ovrEye_Count; i++)
	{
		ovrFovPort* eye = &desc.DefaultEyeFov[i];
		g_VRSystem->GetProjectionRaw((vr::EVREye)i, &eye->LeftTan, &eye->RightTan, &eye->UpTan, &eye->DownTan);
		desc.MaxEyeFov[i] = *eye;
	}

	// Get display properties
	g_VRSystem->GetRecommendedRenderTargetSize((uint32_t*)&desc.Resolution.w, (uint32_t*)&desc.Resolution.h);
	desc.DisplayRefreshRate = g_VRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

	return desc;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackerCount(ovrSession session)
{
	return g_VRSystem->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, nullptr, 0);
}

OVR_PUBLIC_FUNCTION(ovrTrackerDesc) ovr_GetTrackerDesc(ovrSession session, unsigned int trackerDescIndex)
{
	// Get the index for this tracker.
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount];
	g_VRSystem->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, trackers, vr::k_unMaxTrackedDeviceCount);
	vr::TrackedDeviceIndex_t index = trackers[trackerDescIndex];

	// Fill the descriptor
	ovrTrackerDesc desc;

	// Calculate field-of-view
	float left = g_VRSystem->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewLeftDegrees_Float);
	float right = g_VRSystem->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewRightDegrees_Float);
	float top = g_VRSystem->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewTopDegrees_Float);
	float bottom = g_VRSystem->GetFloatTrackedDeviceProperty(index, vr::Prop_FieldOfViewBottomDegrees_Float);
	desc.FrustumHFovInRadians = OVR::DegreeToRad(left + right);
	desc.FrustumVFovInRadians = OVR::DegreeToRad(top + bottom);

	// Get range
	desc.FrustumNearZInMeters = g_VRSystem->GetFloatTrackedDeviceProperty(index, vr::Prop_TrackingRangeMinimumMeters_Float);
	desc.FrustumFarZInMeters = g_VRSystem->GetFloatTrackedDeviceProperty(index, vr::Prop_TrackingRangeMaximumMeters_Float);

	return desc;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Create(ovrSession* pSession, ovrGraphicsLuid* pLuid)
{
	// Initialize the opaque pointer with our own OpenVR-specific struct
	ovrSession session = new struct ovrHmdStruct();
	session->compositor = (vr::IVRCompositor*)VR_GetGenericInterface(vr::IVRCompositor_Version, &g_InitError);

	// Get the LUID for the adapter
	int32_t index;
	IDXGIAdapter* adapter;
	DXGI_ADAPTER_DESC desc;
	g_VRSystem->GetDXGIOutputInfo(&index);
	HRESULT hr = g_pFactory->EnumAdapters(index, &adapter);
	if (FAILED(hr))
		return ovrError_MismatchedAdapters;

	hr = adapter->GetDesc(&desc);
	if (FAILED(hr))
		return ovrError_MismatchedAdapters;

	// Copy the LUID into the structure
	memcpy(pLuid, &desc.AdapterLuid, sizeof(LUID));

	return REV_InitErrorToOvrError(g_InitError);
}

OVR_PUBLIC_FUNCTION(void) ovr_Destroy(ovrSession session)
{
	delete session;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetSessionStatus(ovrSession session, ovrSessionStatus* sessionStatus)
{
	// Fill in the status
	sessionStatus->IsVisible = session->compositor->CanRenderScene();
	sessionStatus->HmdPresent = g_VRSystem->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd);
	sessionStatus->HmdMounted = sessionStatus->HmdPresent;
	sessionStatus->DisplayLost = false;
	sessionStatus->ShouldQuit = false;
	sessionStatus->ShouldRecenter = false;

	// Check for quit event
	// TODO: Should we poll for events here?
	/*vr::VREvent_t ev;
	while (g_VRSystem->PollNextEvent(&ev, sizeof(vr::VREvent_t)))
	{
		if (ev.eventType == vr::VREvent_Quit)
		{
			sessionStatus->ShouldQuit = true;
			g_VRSystem->AcknowledgeQuit_Exiting();
		}
	}*/

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetTrackingOriginType(ovrSession session, ovrTrackingOrigin origin)
{
	// Both enums match exactly, so we can just cast them
	session->compositor->SetTrackingSpace((vr::ETrackingUniverseOrigin)origin);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrTrackingOrigin) ovr_GetTrackingOriginType(ovrSession session)
{
	// Both enums match exactly, so we can just cast them
	return (ovrTrackingOrigin)session->compositor->GetTrackingSpace();
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RecenterTrackingOrigin(ovrSession session)
{
	g_VRSystem->ResetSeatedZeroPose();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_ClearShouldRecenterFlag(ovrSession session) { /* No such flag, do nothing */ }

ovrPoseStatef REV_TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, double time)
{
	ovrPoseStatef result;

	OVR::Matrix4f matrix = REV_HmdMatrixToOVRMatrix(pose.mDeviceToAbsoluteTracking);
	result.ThePose.Orientation = OVR::Quatf(matrix);
	result.ThePose.Position = matrix.GetTranslation();
	result.AngularVelocity = REV_HmdVectorToOVRVector(pose.vAngularVelocity);
	result.LinearVelocity = REV_HmdVectorToOVRVector(pose.vVelocity);
	// TODO: Calculate acceleration.
	result.AngularAcceleration = ovrVector3f();
	result.LinearAcceleration = ovrVector3f();

	return result;
}

unsigned int REV_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose)
{
	unsigned int result = 0;

	if (pose.bPoseIsValid)
	{
		if (pose.bDeviceIsConnected)
			result |= ovrStatus_OrientationTracked;
		if (pose.eTrackingResult != vr::TrackingResult_Calibrating_OutOfRange &&
			pose.eTrackingResult != vr::TrackingResult_Running_OutOfRange)
			result |= ovrStatus_PositionTracked;
	}

	return result;
}

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingState(ovrSession session, double absTime, ovrBool latencyMarker)
{
	ovrTrackingState state;

	// Get the absolute tracking poses
	float time = ovr_GetTimeInSeconds();
	vr::ETrackingUniverseOrigin origin = session->compositor->GetTrackingSpace();
	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	g_VRSystem->GetDeviceToAbsoluteTrackingPose(origin, (float)absTime, poses, vr::k_unMaxTrackedDeviceCount);

	// Convert the head pose
	state.HeadPose = REV_TrackedDevicePoseToOVRPose(poses[vr::k_unTrackedDeviceIndex_Hmd], time);
	state.StatusFlags = REV_TrackedDevicePoseToOVRStatusFlags(poses[vr::k_unTrackedDeviceIndex_Hmd]);

	// Convert the hand poses
	vr::TrackedDeviceIndex_t hands[] = { g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),
		g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) };
	for (int i = 0; i < ovrHand_Count; i++)
	{
		vr::TrackedDeviceIndex_t deviceIndex = hands[i];
		state.HandPoses[i] = REV_TrackedDevicePoseToOVRPose(poses[deviceIndex], time);
		state.HandStatusFlags[i] = REV_TrackedDevicePoseToOVRStatusFlags(poses[deviceIndex]);
	}

	// TODO: It looks like this should be set to GetSeatedZeroPoseToStandingAbsoluteTrackingPose()?
	state.CalibratedOrigin.Orientation = OVR::Quatf();
	state.CalibratedOrigin.Position = OVR::Vector3f();

	return state;
}

OVR_PUBLIC_FUNCTION(ovrTrackerPose) ovr_GetTrackerPose(ovrSession session, unsigned int trackerPoseIndex)
{
	ovrTrackerPose pose;

	// Get the index for this tracker.
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount];
	g_VRSystem->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, trackers, vr::k_unMaxTrackedDeviceCount);
	vr::TrackedDeviceIndex_t index = trackers[trackerPoseIndex];

	// Get the absolute tracking poses
	vr::ETrackingUniverseOrigin origin = session->compositor->GetTrackingSpace();
	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	g_VRSystem->GetDeviceToAbsoluteTrackingPose(origin, 0.0, poses, vr::k_unMaxTrackedDeviceCount);

	// Set the flags
	pose.TrackerFlags = 0;
	if (poses[index].bDeviceIsConnected)
		pose.TrackerFlags |= ovrTracker_Connected;
	if (poses[index].bPoseIsValid)
		pose.TrackerFlags |= ovrTracker_PoseTracked;

	// Convert the pose
	OVR::Matrix4f matrix = REV_HmdMatrixToOVRMatrix(poses[index].mDeviceToAbsoluteTracking);
	OVR::Quatf quat = OVR::Quatf(matrix);
	pose.Pose.Orientation = quat;
	pose.Pose.Position = matrix.GetTranslation();

	// Level the pose
	float yaw;
	quat.GetYawPitchRoll(&yaw, nullptr, nullptr);
	pose.LeveledPose.Orientation = OVR::Quatf(OVR::Axis_Y, yaw);
	pose.LeveledPose.Position = matrix.GetTranslation();

	return pose;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	inputState->TimeInSeconds = ovr_GetTimeInSeconds();

	if (controllerType == ovrControllerType_XBox)
	{
		// Use XInput for Xbox controllers
		XINPUT_STATE input;
		if (XInputGetState(0, &input) == ERROR_SUCCESS)
		{
			// Convert the buttons
			WORD buttons = input.Gamepad.wButtons;
			inputState->Buttons = 0;
			if (buttons & XINPUT_GAMEPAD_DPAD_UP)
				inputState->Buttons |= ovrButton_Up;
			if (buttons & XINPUT_GAMEPAD_DPAD_DOWN)
				inputState->Buttons |= ovrButton_Down;
			if (buttons & XINPUT_GAMEPAD_DPAD_LEFT)
				inputState->Buttons |= ovrButton_Left;
			if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT)
				inputState->Buttons |= ovrButton_Right;
			if (buttons & XINPUT_GAMEPAD_START)
				inputState->Buttons |= ovrButton_Enter;
			if (buttons & XINPUT_GAMEPAD_BACK)
				inputState->Buttons |= ovrButton_Back;
			if (buttons & XINPUT_GAMEPAD_LEFT_THUMB)
				inputState->Buttons |= ovrButton_LThumb;
			if (buttons & XINPUT_GAMEPAD_RIGHT_THUMB)
				inputState->Buttons |= ovrButton_RThumb;
			if (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER)
				inputState->Buttons |= ovrButton_LShoulder;
			if (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
				inputState->Buttons |= ovrButton_RShoulder;
			if (buttons & XINPUT_GAMEPAD_A)
				inputState->Buttons |= ovrButton_A;
			if (buttons & XINPUT_GAMEPAD_B)
				inputState->Buttons |= ovrButton_B;
			if (buttons & XINPUT_GAMEPAD_X)
				inputState->Buttons |= ovrButton_X;
			if (buttons & XINPUT_GAMEPAD_Y)
				inputState->Buttons |= ovrButton_Y;

			// Convert the axes
			inputState->IndexTrigger[ovrHand_Left] = input.Gamepad.bLeftTrigger / 255.0f;
			inputState->IndexTrigger[ovrHand_Right] = input.Gamepad.bRightTrigger / 255.0f;
			inputState->Thumbstick[ovrHand_Left].x = input.Gamepad.sThumbLX / 32768.0f;
			inputState->Thumbstick[ovrHand_Left].y = input.Gamepad.sThumbLY / 32768.0f;
			inputState->Thumbstick[ovrHand_Right].x = input.Gamepad.sThumbRX / 32768.0f;
			inputState->Thumbstick[ovrHand_Right].y = input.Gamepad.sThumbRY / 32768.0f;

			inputState->ControllerType = ovrControllerType_XBox;
			return ovrSuccess;
		}
	}

	// TODO: Implement Oculus Touch support.

	return ovrError_DeviceUnavailable;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetConnectedControllerTypes(ovrSession session) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainLength(ovrSession session, ovrTextureSwapChain chain, int* out_Length) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainCurrentIndex(ovrSession session, ovrTextureSwapChain chain, int* out_Index) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainDesc(ovrSession session, ovrTextureSwapChain chain, ovrTextureSwapChainDesc* out_Desc) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CommitTextureSwapChain(ovrSession session, ovrTextureSwapChain chain) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(void) ovr_DestroyTextureSwapChain(ovrSession session, ovrTextureSwapChain chain) { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(void) ovr_DestroyMirrorTexture(ovrSession session, ovrMirrorTexture mirrorTexture) { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(ovrSizei) ovr_GetFovTextureSize(ovrSession session, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel) { REV_UNIMPLEMENTED_STRUCT(ovrSizei); }

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovr_GetRenderDesc(ovrSession session, ovrEyeType eyeType, ovrFovPort fov) { REV_UNIMPLEMENTED_STRUCT(ovrEyeRenderDesc); }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(double) ovr_GetPredictedDisplayTime(ovrSession session, long long frameIndex) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds()
{
	// Get time in seconds
	float time;
	uint64_t frame;
	g_VRSystem->GetTimeSinceLastVsync(&time, &frame);
	return time;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_GetBool(ovrSession session, const char* propertyName, ovrBool defaultVal) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetBool(ovrSession session, const char* propertyName, ovrBool value) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(int) ovr_GetInt(ovrSession session, const char* propertyName, int defaultVal) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetInt(ovrSession session, const char* propertyName, int value) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(float) ovr_GetFloat(ovrSession session, const char* propertyName, float defaultVal) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloat(ovrSession session, const char* propertyName, float value) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetFloatArray(ovrSession session, const char* propertyName, float values[], unsigned int valuesCapacity) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloatArray(ovrSession session, const char* propertyName, const float values[], unsigned int valuesSize) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(const char*) ovr_GetString(ovrSession session, const char* propertyName, const char* defaultVal) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetString(ovrSession session, const char* propertyName, const char* value) { REV_UNIMPLEMENTED_NULL; }
