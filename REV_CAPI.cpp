#include "OVR_CAPI.h"
#include "OVR_Version.h"
#include "Extras/OVR_Math.h"

#include "openvr.h"
#include "MinHook.h"
#include <DXGI.h>
#include <d3d11.h>
#include <Xinput.h>

#include "REV_Assert.h"
#include "REV_Common.h"
#include "REV_Error.h"
#include "REV_Math.h"

#define REV_SETTINGS_SECTION "revive"

typedef DWORD(__stdcall* _XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD(__stdcall* _XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

HMODULE g_hXInputLib;
_XInputGetState g_pXInputGetState;
_XInputSetState g_pXInputSetState;

vr::EVRInitError g_InitError = vr::VRInitError_None;
vr::IVRSystem* g_VRSystem = nullptr;
char* g_StringBuffer = nullptr;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params)
{
	MH_QueueDisableHook(GetProcAddress);
	MH_QueueDisableHook(LoadLibraryW);
	MH_QueueDisableHook(OpenEventW);
	MH_ApplyQueued();

	g_hXInputLib = LoadLibraryW(L"xinput1_3.dll");
	if (!g_hXInputLib)
		return ovrError_LibLoad;

	g_pXInputGetState = (_XInputGetState)GetProcAddress(g_hXInputLib, "XInputGetState");
	if (!g_pXInputGetState)
		return ovrError_LibLoad;

	g_pXInputSetState = (_XInputSetState)GetProcAddress(g_hXInputLib, "XInputSetState");
	if (!g_pXInputSetState)
		return ovrError_LibLoad;

	g_VRSystem = vr::VR_Init(&g_InitError, vr::VRApplication_Scene);

	MH_QueueEnableHook(LoadLibraryW);
	MH_QueueEnableHook(OpenEventW);
	MH_ApplyQueued();

	return REV_InitErrorToOvrError(g_InitError);
}

OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{
	// Delete the global string property buffer.
	if (g_StringBuffer)
		delete g_StringBuffer;
	g_StringBuffer = nullptr;

	if (g_hXInputLib)
		FreeLibrary(g_hXInputLib);

	vr::VR_Shutdown();
}

OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo)
{
	const char* error = VR_GetVRInitErrorAsEnglishDescription(g_InitError);
	strncpy_s(errorInfo->ErrorString, error, sizeof(ovrErrorInfo::ErrorString));
	errorInfo->Result = REV_InitErrorToOvrError(g_InitError);
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString()
{
	return OVR_VERSION_STRING;
}

OVR_PUBLIC_FUNCTION(int) ovr_TraceMessage(int level, const char* message) { return 0; /* Debugging feature */ }

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
		ovrFovPort eye;
		g_VRSystem->GetProjectionRaw((vr::EVREye)i, &eye.LeftTan, &eye.RightTan, &eye.DownTan, &eye.UpTan);
		eye.LeftTan *= -1.0f;
		eye.DownTan *= -1.0f;
		desc.DefaultEyeFov[i] = eye;
		desc.MaxEyeFov[i] = eye;
	}

	// Get display properties
	g_VRSystem->GetRecommendedRenderTargetSize((uint32_t*)&desc.Resolution.w, (uint32_t*)&desc.Resolution.h);
	desc.Resolution.w *= 2; // Both eye ports
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
	memset(session->ColorTexture, 0, sizeof(ovrHmdStruct::ColorTexture));

	// Get the compositor interface
	session->compositor = (vr::IVRCompositor*)VR_GetGenericInterface(vr::IVRCompositor_Version, &g_InitError);
	if (g_InitError != vr::VRInitError_None)
		return REV_InitErrorToOvrError(g_InitError);
	session->compositor->SetTrackingSpace(vr::TrackingUniverseSeated);

	// Get the settings interface
	session->settings = (vr::IVRSettings*)VR_GetGenericInterface(vr::IVRSettings_Version, &g_InitError);
	if (g_InitError != vr::VRInitError_None)
		return REV_InitErrorToOvrError(g_InitError);

	// Get the overlay interface
	session->overlay = (vr::IVROverlay*)VR_GetGenericInterface(vr::IVROverlay_Version, &g_InitError);
	if (g_InitError != vr::VRInitError_None)
		return REV_InitErrorToOvrError(g_InitError);

	// Get the LUID for the adapter
	int32_t index;
	g_VRSystem->GetDXGIOutputInfo(&index);
	if (index == -1)
		index = 0;

	// Create the DXGI factory
	IDXGIFactory* pFactory;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&pFactory));
	if (FAILED(hr))
		return ovrError_IncompatibleGPU;

	IDXGIAdapter* pAdapter;
	hr = pFactory->EnumAdapters(index, &pAdapter);
	if (FAILED(hr))
		return ovrError_MismatchedAdapters;

	DXGI_ADAPTER_DESC desc;
	hr = pAdapter->GetDesc(&desc);
	if (FAILED(hr))
		return ovrError_MismatchedAdapters;

	// Copy the LUID into the structure
	memcpy(pLuid, &desc.AdapterLuid, sizeof(LUID));

	// Cleanup and return
	pFactory->Release();
	pAdapter->Release();
	*pSession = session;
	return ovrSuccess;
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
	ovrPoseStatef result = { 0 };
	result.ThePose = OVR::Posef::Identity();

	OVR::Matrix4f matrix;
	if (pose.bPoseIsValid)
		matrix = REV_HmdMatrixToOVRMatrix(pose.mDeviceToAbsoluteTracking);
	else
		return result;

	result.ThePose.Orientation = OVR::Quatf(matrix);
	result.ThePose.Position = matrix.GetTranslation();
	result.AngularVelocity = REV_HmdVectorToOVRVector(pose.vAngularVelocity);
	result.LinearVelocity = REV_HmdVectorToOVRVector(pose.vVelocity);
	// TODO: Calculate acceleration.
	result.AngularAcceleration = ovrVector3f();
	result.LinearAcceleration = ovrVector3f();
	result.TimeInSeconds = time;

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
	ovrTrackingState state = { 0 };

	// Gain focus for the compositor
	float time = (float)ovr_GetTimeInSeconds();

	// Get the absolute tracking poses
	vr::TrackedDevicePose_t* poses = session->gamePoses;

	// Convert the head pose
	state.HeadPose = REV_TrackedDevicePoseToOVRPose(poses[vr::k_unTrackedDeviceIndex_Hmd], time);
	state.StatusFlags = REV_TrackedDevicePoseToOVRStatusFlags(poses[vr::k_unTrackedDeviceIndex_Hmd]);

	// Convert the hand poses
	vr::TrackedDeviceIndex_t hands[] = { g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),
		g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) };
	for (int i = 0; i < ovrHand_Count; i++)
	{
		vr::TrackedDeviceIndex_t deviceIndex = hands[i];
		if (deviceIndex == (uint32_t)-1)
		{
			state.HandPoses[i].ThePose = OVR::Posef::Identity();
			continue;
		}

		state.HandPoses[i] = REV_TrackedDevicePoseToOVRPose(poses[deviceIndex], time);
		state.HandStatusFlags[i] = REV_TrackedDevicePoseToOVRStatusFlags(poses[deviceIndex]);
	}

	// TODO: Should this transformation be inverted?
	OVR::Matrix4f origin = REV_HmdMatrixToOVRMatrix(g_VRSystem->GetSeatedZeroPoseToStandingAbsoluteTrackingPose());
	state.CalibratedOrigin.Orientation = OVR::Quatf(origin);
	state.CalibratedOrigin.Position = origin.GetTranslation();

	return state;
}

OVR_PUBLIC_FUNCTION(ovrTrackerPose) ovr_GetTrackerPose(ovrSession session, unsigned int trackerPoseIndex)
{
	ovrTrackerPose pose = { 0 };

	// Get the index for this tracker.
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount];
	g_VRSystem->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, trackers, vr::k_unMaxTrackedDeviceCount);
	vr::TrackedDeviceIndex_t index = trackers[trackerPoseIndex];

	// Set the flags
	pose.TrackerFlags = 0;
	if (session->poses[index].bDeviceIsConnected)
		pose.TrackerFlags |= ovrTracker_Connected;
	if (session->poses[index].bPoseIsValid)
		pose.TrackerFlags |= ovrTracker_PoseTracked;

	// Convert the pose
	OVR::Matrix4f matrix;
	if (session->poses[index].bPoseIsValid)
		matrix = REV_HmdMatrixToOVRMatrix(session->poses[index].mDeviceToAbsoluteTracking);
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
	memset(inputState, 0, sizeof(ovrInputState));

	inputState->TimeInSeconds = ovr_GetTimeInSeconds();
	inputState->ControllerType = controllerType;

	if (controllerType & ovrControllerType_Touch)
	{
		vr::TrackedDeviceIndex_t hands[] = { g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand),
			g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand) };

		for (int i = 0; i < ovrHand_Count; i++)
		{
			if (hands[i] != (uint32_t)-1)
			{
				unsigned int buttons = 0, touches = 0;
				vr::VRControllerState_t state;
				g_VRSystem->GetControllerState(hands[i], &state);

				if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
					buttons |= ovrButton_Home;

				if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
					buttons |= ovrButton_RShoulder;

				if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip))
					inputState->HandTrigger[i] = 1.0f;

				if (!(state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)))
					touches |= ovrTouch_RThumbUp;

				if (!(state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)))
					touches |= ovrTouch_RIndexPointing;

				// Convert the axes
				for (int j = 0; j < vr::k_unControllerStateAxisCount; j++)
				{
					vr::ETrackedDeviceProperty prop = (vr::ETrackedDeviceProperty)(vr::Prop_Axis0Type_Int32 + j);
					vr::EVRControllerAxisType type = (vr::EVRControllerAxisType)g_VRSystem->GetInt32TrackedDeviceProperty(hands[i], prop);
					vr::VRControllerAxis_t axis = state.rAxis[j];

					if (type == vr::k_eControllerAxis_TrackPad)
					{
						if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
						{
							float magnitude = sqrt(axis.x*axis.x + axis.y*axis.y);

							// Only the center of the touchpad is a thumb stick
							if (magnitude > 0.5f)
							{
								if (axis.x > 0.0f)
									buttons |= ovrButton_A;
								else
									buttons |= ovrButton_B;
							}
							else
							{
								buttons |= ovrButton_LThumb;
							}
						}
						else if (state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
						{
							// Map the touchpad to the thumbstick
							inputState->Thumbstick[i].x = axis.x;
							inputState->Thumbstick[i].y = axis.y;
						}
					}

					if (type == vr::k_eControllerAxis_Trigger &&
						state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
						inputState->IndexTrigger[i] = axis.x;
				}

				// Commit buttons/touches and bitshift it for the left hand.
				inputState->Buttons |= buttons << (100 * i);
				inputState->Touches |= touches << (100 * i);
			}
		}
	}

	// Use XInput for Xbox controllers
	if (controllerType & ovrControllerType_XBox)
	{
		XINPUT_STATE state;
		if (g_pXInputGetState(0, &state) == ERROR_SUCCESS)
		{
			// Convert the buttons
			WORD buttons = state.Gamepad.wButtons;
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
			SHORT deadzones[] = { XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE };
			for (int i = 0; i < ovrHand_Count; i++)
			{
				float X, Y, trigger;
				if (i == ovrHand_Left)
				{
					X = state.Gamepad.sThumbLX;
					Y = state.Gamepad.sThumbLY;
					trigger = state.Gamepad.bLeftTrigger;
				}
				if (i == ovrHand_Right)
				{
					X = state.Gamepad.sThumbRX;
					Y = state.Gamepad.sThumbRY;
					trigger = state.Gamepad.bRightTrigger;
				}

				//determine how far the controller is pushed
				float magnitude = sqrt(X*X + Y*Y);

				//determine the direction the controller is pushed
				float normalizedX = X / magnitude;
				float normalizedY = Y / magnitude;

				//check if the controller is outside a circular dead zone
				if (magnitude > deadzones[i])
				{
					//clip the magnitude at its expected maximum value
					if (magnitude > 32767) magnitude = 32767;

					//adjust magnitude relative to the end of the dead zone
					magnitude -= deadzones[i];

					//optionally normalize the magnitude with respect to its expected range
					//giving a magnitude value of 0.0 to 1.0
					float normalizedMagnitude = magnitude / (32767 - deadzones[i]);
					inputState->Thumbstick[i].x = normalizedMagnitude * normalizedX;
					inputState->Thumbstick[i].y = normalizedMagnitude * normalizedY;
				}

				if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
				{
					//clip the magnitude at its expected maximum value
					if (trigger > 255) trigger = 255;

					//adjust magnitude relative to the end of the dead zone
					trigger -= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

					//optionally normalize the magnitude with respect to its expected range
					//giving a magnitude value of 0.0 to 1.0
					float normalizedTrigger = trigger / (255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
					inputState->IndexTrigger[i] = normalizedTrigger;
				}
			}
		}
	}

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetConnectedControllerTypes(ovrSession session)
{
	unsigned int types = 0;

	// Check for Vive controllers
	if (g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand) != (uint32_t)-1)
		types |= ovrControllerType_LTouch;

	if (g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) != (uint32_t)-1)
		types |= ovrControllerType_RTouch;

	// Check for Xbox controller
	XINPUT_STATE input;
	if (g_pXInputGetState(0, &input) == ERROR_SUCCESS)
	{
		types |= ovrControllerType_XBox;
	}

	return types;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	// TODO: Disable the rumbler after a nominal amount of time.
	// TODO: Implement Oculus Touch support.

	if (controllerType == ovrControllerType_XBox)
	{
		XINPUT_VIBRATION vibration;
		ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));
		if (frequency > 0.0f)
		{
			// The right motor is the high-frequency motor, the left motor is the low-frequency motor.
			if (frequency > 0.5f)
				vibration.wRightMotorSpeed = WORD(65535.0f * amplitude);
			else
				vibration.wLeftMotorSpeed = WORD(65535.0f * amplitude);
		}
		g_pXInputSetState(0, &vibration);

		return ovrSuccess;
	}

	return ovrError_DeviceUnavailable;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainLength(ovrSession session, ovrTextureSwapChain chain, int* out_Length)
{
	*out_Length = chain->length;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainCurrentIndex(ovrSession session, ovrTextureSwapChain chain, int* out_Index)
{
	*out_Index = chain->index;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainDesc(ovrSession session, ovrTextureSwapChain chain, ovrTextureSwapChainDesc* out_Desc)
{
	out_Desc = &chain->desc;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CommitTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	chain->current = chain->texture[chain->index];
	chain->index++;
	chain->index %= chain->length;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	if (!chain)
		return;

	if (chain->texture[0].eType == vr::API_DirectX)
	{
		for (int i = 0; i < chain->length; i++)
			((ID3D11Texture2D*)chain->texture[i].handle)->Release();
	}

	delete chain;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyMirrorTexture(ovrSession session, ovrMirrorTexture mirrorTexture)
{
	if (!mirrorTexture)
		return;

	if (mirrorTexture->texture.eType == vr::API_DirectX)
		((ID3D11Texture2D*)mirrorTexture->texture.handle)->Release();

	delete mirrorTexture;
}

OVR_PUBLIC_FUNCTION(ovrSizei) ovr_GetFovTextureSize(ovrSession session, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel)
{
	ovrSizei size;
	g_VRSystem->GetRecommendedRenderTargetSize((uint32_t*)&size.w, (uint32_t*)&size.h);

	float left, right, top, bottom;
	g_VRSystem->GetProjectionRaw((vr::EVREye)eye, &left, &right, &top, &bottom);

	float uMin = 0.5f + 0.5f * left / fov.LeftTan;
	float uMax = 0.5f + 0.5f * right / fov.RightTan;
	float vMin = 0.5f - 0.5f * bottom / fov.DownTan;
	float vMax = 0.5f - 0.5f * top / fov.UpTan;

	// Grow the recommended size to account for the overlapping fov
	size.w = int(size.w / (uMax - uMin));
	size.h = int(size.h / (vMax - vMin));

	return size;
}

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovr_GetRenderDesc(ovrSession session, ovrEyeType eyeType, ovrFovPort fov)
{
	ovrEyeRenderDesc desc;
	desc.Eye = eyeType;
	desc.Fov = fov;

	OVR::Matrix4f HmdToEyeMatrix = REV_HmdMatrixToOVRMatrix(g_VRSystem->GetEyeToHeadTransform((vr::EVREye)eyeType));
	float WidthTan = fov.LeftTan + fov.RightTan;
	float HeightTan = fov.UpTan + fov.DownTan;
	ovrSizei size;
	g_VRSystem->GetRecommendedRenderTargetSize((uint32_t*)&size.w, (uint32_t*)&size.h);

	desc.DistortedViewport = OVR::Recti(eyeType == ovrEye_Right ? size.w : 0, 0, size.w, size.h);
	desc.PixelsPerTanAngleAtCenter = OVR::Vector2f(size.w / WidthTan, size.h / HeightTan);
	desc.HmdToEyeOffset = HmdToEyeMatrix.GetTranslation();

	return desc;
}

vr::VRTextureBounds_t REV_ViewportToTextureBounds(ovrRecti viewport, ovrTextureSwapChain swapChain, unsigned int flags)
{
	vr::VRTextureBounds_t bounds;
	float w = (float)swapChain->desc.Width;
	float h = (float)swapChain->desc.Height;
	bounds.uMin = viewport.Pos.x / w;
	bounds.vMin = viewport.Pos.y / h;

	// Sanity check for the viewport size.
	// Workaround for Defense Grid 2, which leaves these variables unintialized.
	if (viewport.Size.w > 0 && viewport.Size.h > 0)
	{
		bounds.uMax = (viewport.Pos.x + viewport.Size.w) / w;
		bounds.vMax = (viewport.Pos.y + viewport.Size.h) / h;
	}
	else
	{
		bounds.uMax = 1.0f;
		bounds.vMax = 1.0f;
	}

	if (flags & ovrLayerFlag_TextureOriginAtBottomLeft)
	{
		bounds.vMin = 1.0f - bounds.vMin;
		bounds.vMax = 1.0f - bounds.vMax;
	}

	return bounds;
}

vr::HmdMatrix34_t REV_OvrPoseToHmdMatrix(ovrPosef pose)
{
	vr::HmdMatrix34_t result;
	OVR::Matrix4f matrix(pose);
	memcpy(result.m, matrix.M, sizeof(result.m));
	return result;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	// TODO: Implement scaling through ApplyTransform().

	// Call WaitGetPoses() to do some cleanup from the previous frame.
	session->compositor->WaitGetPoses(session->poses, vr::k_unMaxTrackedDeviceCount, session->gamePoses, vr::k_unMaxTrackedDeviceCount);

	if (layerCount == 0)
		return ovrError_InvalidParameter;

	// Other layers are interpreted as overlays.
	for (size_t i = 0; i < ovrMaxLayerCount; i++)
	{
		char keyName[vr::k_unVROverlayMaxKeyLength];
		snprintf(keyName, vr::k_unVROverlayMaxKeyLength, "Revive_%d", i);

		// Look if the overlay already exists.
		vr::VROverlayHandle_t overlay;
		vr::EVROverlayError err = session->overlay->FindOverlay(keyName, &overlay);

		// If this layer is defined in the list, show it. If not, hide the layer if it exists.
		if (i < layerCount)
		{
			// Overlays are assumed to be monoscopic quads.
			if (layerPtrList[i]->Type != ovrLayerType_Quad)
				continue;

			// Create a new overlay if it doesn't exist.
			if (err == vr::VROverlayError_UnknownOverlay)
			{
				char title[vr::k_unVROverlayMaxNameLength];
				snprintf(title, vr::k_unVROverlayMaxNameLength, "Revive Layer %d", i);
				session->overlay->CreateOverlay(keyName, title, &overlay);
			}

			// A layer was added, but not defined, hide it.
			if (layerPtrList[i] == nullptr)
			{
				session->overlay->HideOverlay(overlay);
				continue;
			}

			ovrLayerQuad* layer = (ovrLayerQuad*)layerPtrList[i];

			// Set the high quality overlay.
			// FIXME: Why are High quality overlays headlocked in OpenVR?
			if (layer->Header.Flags & (ovrLayerFlag_HighQuality | ovrLayerFlag_HeadLocked))
				session->overlay->SetHighQualityOverlay(overlay);

			// Transform the overlay.
			vr::HmdMatrix34_t transform = REV_OvrPoseToHmdMatrix(layer->QuadPoseCenter);
			session->overlay->SetOverlayWidthInMeters(overlay, layer->QuadSize.x);
			if (layer->Header.Flags & ovrLayerFlag_HeadLocked)
				session->overlay->SetOverlayTransformTrackedDeviceRelative(overlay, vr::k_unTrackedDeviceIndex_Hmd, &transform);
			else
				session->overlay->SetOverlayTransformAbsolute(overlay, session->compositor->GetTrackingSpace(), &transform);

			// Set the texture and show the overlay.
			ovrTextureSwapChain chain = layer->ColorTexture;
			vr::VRTextureBounds_t bounds = REV_ViewportToTextureBounds(layer->Viewport, layer->ColorTexture, layer->Header.Flags);
			session->overlay->SetOverlayTextureBounds(overlay, &bounds);
			session->overlay->SetOverlayTexture(overlay, &chain->current);

			// TODO: Handle overlay errors.
			session->overlay->ShowOverlay(overlay);
		}
		else
		{
			if (err == vr::VROverlayError_UnknownOverlay)
				break;

			// Hide all overlays no longer visible.
			session->overlay->HideOverlay(overlay);
		}
	}

	// The first layer is assumed to be the application scene.
	if (layerPtrList[0]->Type == ovrLayerType_EyeFov)
	{
		ovrLayerEyeFov* sceneLayer = (ovrLayerEyeFov*)layerPtrList[0];

		// Submit the scene layer.
		for (int i = 0; i < ovrEye_Count; i++)
		{
			ovrTextureSwapChain chain = sceneLayer->ColorTexture[i];
			session->ColorTexture[i] = chain;
			vr::VRTextureBounds_t bounds = REV_ViewportToTextureBounds(sceneLayer->Viewport[i], sceneLayer->ColorTexture[i], sceneLayer->Header.Flags);

			float left, right, top, bottom;
			g_VRSystem->GetProjectionRaw((vr::EVREye)i, &left, &right, &top, &bottom);

			// Shrink the bounds to account for the overlapping fov
			ovrFovPort fov = sceneLayer->Fov[i];
			float uMin = 0.5f + 0.5f * left / fov.LeftTan;
			float uMax = 0.5f + 0.5f * right / fov.RightTan;
			float vMin = 0.5f - 0.5f * bottom / fov.DownTan;
			float vMax = 0.5f - 0.5f * top / fov.UpTan;

			// Combine the fov bounds with the viewport bounds
			bounds.uMin += uMin * bounds.uMax;
			bounds.uMax *= uMax;
			bounds.vMin += vMin * bounds.vMax;
			bounds.vMax *= vMax;

			vr::EVRCompositorError err = session->compositor->Submit((vr::EVREye)i, &chain->current, &bounds);
			if (err != vr::VRCompositorError_None)
				return REV_CompositorErrorToOvrError(err);
		}
	}

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetPredictedDisplayTime(ovrSession session, long long frameIndex)
{
	float fDisplayFrequency = g_VRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
	float fVsyncToPhotons = g_VRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);

	return double(frameIndex) / fDisplayFrequency + fVsyncToPhotons;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds()
{
	float fDisplayFrequency = g_VRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

	// Get time in seconds
	float fSecondsSinceLastVsync;
	uint64_t unFrame;
	g_VRSystem->GetTimeSinceLastVsync(&fSecondsSinceLastVsync, &unFrame);

	return double(unFrame) / fDisplayFrequency + fSecondsSinceLastVsync;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_GetBool(ovrSession session, const char* propertyName, ovrBool defaultVal)
{
	return session->settings->GetBool(REV_SETTINGS_SECTION, propertyName, !!defaultVal);
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetBool(ovrSession session, const char* propertyName, ovrBool value)
{
	vr::EVRSettingsError error;
	session->settings->SetBool(REV_SETTINGS_SECTION, propertyName, !!value, &error);
	session->settings->Sync();
	return error == vr::VRSettingsError_None;
}

OVR_PUBLIC_FUNCTION(int) ovr_GetInt(ovrSession session, const char* propertyName, int defaultVal)
{
	return session->settings->GetInt32(REV_SETTINGS_SECTION, propertyName, defaultVal);
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetInt(ovrSession session, const char* propertyName, int value)
{
	vr::EVRSettingsError error;
	session->settings->SetInt32(REV_SETTINGS_SECTION, propertyName, value, &error);
	session->settings->Sync();
	return error == vr::VRSettingsError_None;
}

OVR_PUBLIC_FUNCTION(float) ovr_GetFloat(ovrSession session, const char* propertyName, float defaultVal)
{
	return session->settings->GetFloat(REV_SETTINGS_SECTION, propertyName, defaultVal);
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloat(ovrSession session, const char* propertyName, float value)
{
	vr::EVRSettingsError error;
	session->settings->SetFloat(REV_SETTINGS_SECTION, propertyName, value, &error);
	session->settings->Sync();
	return error == vr::VRSettingsError_None;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetFloatArray(ovrSession session, const char* propertyName, float values[], unsigned int valuesCapacity)
{
	char key[vr::k_unMaxSettingsKeyLength] = { 0 };

	for (size_t i = 0; i < valuesCapacity; i++)
	{
		vr::EVRSettingsError error;
		snprintf(key, vr::k_unMaxSettingsKeyLength, "%s[%d]", propertyName, i);
		values[i] = session->settings->GetFloat(REV_SETTINGS_SECTION, key, 0.0f, &error);

		if (error != vr::VRSettingsError_None)
			return i;
	}

	return valuesCapacity;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloatArray(ovrSession session, const char* propertyName, const float values[], unsigned int valuesSize)
{
	char key[vr::k_unMaxSettingsKeyLength] = { 0 };

	for (size_t i = 0; i < valuesSize; i++)
	{
		vr::EVRSettingsError error;
		snprintf(key, vr::k_unMaxSettingsKeyLength, "%s[%d]", propertyName, i);
		session->settings->SetFloat(REV_SETTINGS_SECTION, key, values[i], &error);

		if (error != vr::VRSettingsError_None)
			return false;
	}

	session->settings->Sync();
	return true;
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetString(ovrSession session, const char* propertyName, const char* defaultVal)
{
	if (g_StringBuffer == nullptr)
		g_StringBuffer = new char[vr::k_unMaxPropertyStringSize];

	session->settings->GetString(REV_SETTINGS_SECTION, propertyName, g_StringBuffer, vr::k_unMaxPropertyStringSize, defaultVal);
	return g_StringBuffer;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetString(ovrSession session, const char* propertyName, const char* value)
{
	vr::EVRSettingsError error;
	session->settings->SetString(REV_SETTINGS_SECTION, propertyName, value, &error);
	session->settings->Sync();
	return error == vr::VRSettingsError_None;
}
