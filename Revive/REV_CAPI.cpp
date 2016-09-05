#include "OVR_CAPI.h"
#include "OVR_Version.h"
#include "Extras/OVR_Math.h"

#include "openvr.h"
#include "MinHook.h"
#include <DXGI.h>
#include <Xinput.h>
#include <vector>
#include <algorithm>

#include "REV_Assert.h"
#include "REV_Common.h"
#include "REV_Error.h"
#include "REV_Math.h"

#define REV_SETTINGS_SECTION "revive"
#define REV_LAYER_BIAS 0.0001f

typedef DWORD(__stdcall* _XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD(__stdcall* _XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

HMODULE g_hXInputLib;
_XInputGetState g_pXInputGetState;
_XInputSetState g_pXInputSetState;

vr::EVRInitError g_InitError = vr::VRInitError_None;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params)
{
	REV_TRACE(ovr_Initialize);

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

	vr::VR_Init(&g_InitError, vr::VRApplication_Scene);

	MH_QueueEnableHook(LoadLibraryW);
	MH_QueueEnableHook(OpenEventW);
	MH_ApplyQueued();

	return rev_InitErrorToOvrError(g_InitError);
}

OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{
	REV_TRACE(ovr_Shutdown);

	if (g_hXInputLib)
		FreeLibrary(g_hXInputLib);

	vr::VR_Shutdown();
}

OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo)
{
	REV_TRACE(ovr_GetLastErrorInfo);

	if (!errorInfo)
		return;

	const char* error = VR_GetVRInitErrorAsEnglishDescription(g_InitError);
	strncpy_s(errorInfo->ErrorString, error, sizeof(ovrErrorInfo::ErrorString));
	errorInfo->Result = rev_InitErrorToOvrError(g_InitError);
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString()
{
	REV_TRACE(ovr_GetVersionString);

	return OVR_VERSION_STRING;
}

OVR_PUBLIC_FUNCTION(int) ovr_TraceMessage(int level, const char* message) { return 0; /* Debugging feature */ }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_IdentifyClient(const char* identity) { return ovrSuccess; /* Debugging feature */ }

OVR_PUBLIC_FUNCTION(ovrHmdDesc) ovr_GetHmdDesc(ovrSession session)
{
	REV_TRACE(ovr_GetHmdDesc);

	ovrHmdDesc desc;
	desc.Type = ovrHmd_CV1;

	// Get HMD name
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, desc.ProductName, 64);
	vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, desc.Manufacturer, 64);

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

	// Get field-of-view
	for (int i = 0; i < ovrEye_Count; i++)
	{
		ovrFovPort eye;
		vr::VRSystem()->GetProjectionRaw((vr::EVREye)i, &eye.LeftTan, &eye.RightTan, &eye.DownTan, &eye.UpTan);
		eye.LeftTan *= -1.0f;
		eye.DownTan *= -1.0f;
		desc.DefaultEyeFov[i] = eye;
		desc.MaxEyeFov[i] = eye;
	}

	// Get display properties
	vr::VRSystem()->GetRecommendedRenderTargetSize((uint32_t*)&desc.Resolution.w, (uint32_t*)&desc.Resolution.h);
	desc.Resolution.w *= 2; // Both eye ports
	desc.DisplayRefreshRate = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

	return desc;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackerCount(ovrSession session)
{
	REV_TRACE(ovr_GetTrackerCount);

	uint32_t count = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, nullptr, 0);

	return count;
}

OVR_PUBLIC_FUNCTION(ovrTrackerDesc) ovr_GetTrackerDesc(ovrSession session, unsigned int trackerDescIndex)
{
	REV_TRACE(ovr_GetTrackerDesc);

	// Get the index for this tracker.
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount];
	vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, trackers, vr::k_unMaxTrackedDeviceCount);
	vr::TrackedDeviceIndex_t index = trackers[trackerDescIndex];

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

	return desc;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Create(ovrSession* pSession, ovrGraphicsLuid* pLuid)
{
	REV_TRACE(ovr_Create);

	if (!pSession)
		return ovrError_InvalidParameter;

	*pSession = nullptr;

	// Initialize the opaque pointer with our own OpenVR-specific struct
	ovrSession session = new ovrHmdStruct();

	// Get the default universe origin from the settings
	vr::ETrackingUniverseOrigin origin = (vr::ETrackingUniverseOrigin)vr::VRSettings()->GetInt32(REV_SETTINGS_SECTION, "DefaultTrackingOrigin", vr::TrackingUniverseSeated);
	vr::VRCompositor()->SetTrackingSpace(origin);

	// Get the first poses
	vr::VRCompositor()->WaitGetPoses(session->Poses, vr::k_unMaxTrackedDeviceCount, session->GamePoses, vr::k_unMaxTrackedDeviceCount);

	// Apply settings
	session->ThumbStickRange = vr::VRSettings()->GetFloat(REV_SETTINGS_SECTION, "ThumbStickRange", 0.8f);

	// Get the LUID for the default adapter
	int32_t index;
	vr::VRSystem()->GetDXGIOutputInfo(&index);
	if (index == -1)
		index = 0;

	// Create the DXGI factory
	IDXGIFactory* pFactory;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&pFactory));
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
	REV_TRACE(ovr_Destroy);

	delete session;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetSessionStatus(ovrSession session, ovrSessionStatus* sessionStatus)
{
	REV_TRACE(ovr_GetSessionStatus);

	if (!sessionStatus)
		return ovrError_InvalidParameter;

	// Check for quit event
	vr::VREvent_t ev;
	while (vr::VRSystem()->PollNextEvent(&ev, sizeof(vr::VREvent_t)))
	{
		if (ev.eventType == vr::VREvent_Quit)
		{
			session->ShouldQuit = true;
			vr::VRSystem()->AcknowledgeQuit_Exiting();
		}
	}

	// Detect if the application has focus, but only return false the first time the status is requested.
	// If this is true from the beginning then some games will assume the Health-and-Safety warning
	// is still being displayed.
	sessionStatus->IsVisible = vr::VRCompositor()->CanRenderScene() && session->IsVisible;
	session->IsVisible = true;

	// TODO: Use the HMD sensor to detect whether the headset is mounted.
	// TODO: Detect if the display is lost, can this ever happen with OpenVR?
	sessionStatus->HmdPresent = vr::VRSystem()->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd);
	sessionStatus->HmdMounted = sessionStatus->HmdPresent;
	sessionStatus->DisplayLost = false;
	sessionStatus->ShouldQuit = session->ShouldQuit;
	sessionStatus->ShouldRecenter = false;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetTrackingOriginType(ovrSession session, ovrTrackingOrigin origin)
{
	REV_TRACE(ovr_SetTrackingOriginType);

	if (!session)
		return ovrError_InvalidSession;

	// Both enums match exactly, so we can just cast them
	vr::VRCompositor()->SetTrackingSpace((vr::ETrackingUniverseOrigin)origin);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrTrackingOrigin) ovr_GetTrackingOriginType(ovrSession session)
{
	REV_TRACE(ovr_GetTrackingOriginType);

	if (!session)
		return ovrTrackingOrigin_EyeLevel;

	// Both enums match exactly, so we can just cast them
	return (ovrTrackingOrigin)vr::VRCompositor()->GetTrackingSpace();
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RecenterTrackingOrigin(ovrSession session)
{
	REV_TRACE(ovr_RecenterTrackingOrigin);

	if (!session)
		return ovrError_InvalidSession;

	// When an Oculus game recenters the tracking origin it is implied that the tracking origin
	// should now be seated.
	vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseSeated);

	vr::VRSystem()->ResetSeatedZeroPose();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_ClearShouldRecenterFlag(ovrSession session) { /* No such flag, do nothing */ }

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingState(ovrSession session, double absTime, ovrBool latencyMarker)
{
	REV_TRACE(ovr_GetTrackingState);

	ovrTrackingState state = { 0 };

	if (!session)
		return state;

	// Get the absolute tracking poses
	vr::TrackedDevicePose_t* poses = session->Poses;

	// Convert the head pose
	state.HeadPose = rev_TrackedDevicePoseToOVRPose(poses[vr::k_unTrackedDeviceIndex_Hmd], absTime);
	state.StatusFlags = rev_TrackedDevicePoseToOVRStatusFlags(poses[vr::k_unTrackedDeviceIndex_Hmd]);

	// Convert the hand poses
	vr::TrackedDeviceIndex_t hands[] = { vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),
		vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) };
	for (int i = 0; i < ovrHand_Count; i++)
	{
		vr::TrackedDeviceIndex_t deviceIndex = hands[i];
		if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid)
		{
			state.HandPoses[i].ThePose = OVR::Posef::Identity();
			continue;
		}

		state.HandPoses[i] = rev_TrackedDevicePoseToOVRPose(poses[deviceIndex], absTime);
		state.HandStatusFlags[i] = rev_TrackedDevicePoseToOVRStatusFlags(poses[deviceIndex]);
	}

	OVR::Matrix4f origin = rev_HmdMatrixToOVRMatrix(vr::VRSystem()->GetSeatedZeroPoseToStandingAbsoluteTrackingPose());

	// The calibrated origin should be the location of the seated origin relative to the absolute tracking space.
	// It currently describes the location of the absolute origin relative to the seated origin, so we have to invert it.
	origin.Invert();

	state.CalibratedOrigin.Orientation = OVR::Quatf(origin);
	state.CalibratedOrigin.Position = origin.GetTranslation();

	return state;
}

OVR_PUBLIC_FUNCTION(ovrTrackerPose) ovr_GetTrackerPose(ovrSession session, unsigned int trackerPoseIndex)
{
	REV_TRACE(ovr_GetTrackerPose);

	ovrTrackerPose pose = { 0 };

	if (!session)
		return pose;

	// Get the index for this tracker.
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount] = { vr::k_unTrackedDeviceIndexInvalid };
	vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, trackers, vr::k_unMaxTrackedDeviceCount);
	vr::TrackedDeviceIndex_t index = trackers[trackerPoseIndex];

	// Set the flags
	pose.TrackerFlags = 0;
	if (index != vr::k_unTrackedDeviceIndexInvalid)
	{
		if (session->Poses[index].bDeviceIsConnected)
			pose.TrackerFlags |= ovrTracker_Connected;
		if (session->Poses[index].bPoseIsValid)
			pose.TrackerFlags |= ovrTracker_PoseTracked;
	}

	// Convert the pose
	OVR::Matrix4f matrix;
	if (index != vr::k_unTrackedDeviceIndexInvalid && session->Poses[index].bPoseIsValid)
		matrix = rev_HmdMatrixToOVRMatrix(session->Poses[index].mDeviceToAbsoluteTracking);

	// We need to mirror the orientation along either the X or Y axis
	OVR::Quatf quat = OVR::Quatf(matrix);
	OVR::Quatf mirror = OVR::Quatf(1.0f, 0.0f, 0.0f, 0.0f);
	pose.Pose.Orientation = quat * mirror;
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
	REV_TRACE(ovr_GetInputState);

	if (!session)
		return ovrError_InvalidSession;

	if (!inputState)
		return ovrError_InvalidParameter;

	memset(inputState, 0, sizeof(ovrInputState));

	inputState->TimeInSeconds = ovr_GetTimeInSeconds();

	// Get controller indices.
	vr::TrackedDeviceIndex_t controllers[vr::k_unMaxTrackedDeviceCount];
	uint32_t controllerCount = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_Controller, controllers, vr::k_unMaxTrackedDeviceCount);

	if (controllerType & ovrControllerType_Touch)
	{
		// Get controller indices.
		vr::TrackedDeviceIndex_t hands[] = { vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),
			vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) };

		// Only if both controllers are available, then the touch controller is connected.
		if (controllerCount > 1)
		{
			for (int i = 0; i < ovrHand_Count; i++)
			{
				if (hands[i] == vr::k_unTrackedDeviceIndexInvalid)
					continue;

				vr::VRControllerState_t state;
				vr::VRSystem()->GetControllerState(hands[i], &state);

				unsigned int buttons = 0, touches = 0;
				bool isLeft = (i == ovrHand_Left);

				if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
				{
					if (!session->MenuWasPressed[i])
						session->ThumbStick[i] = !session->ThumbStick[i];

					session->MenuWasPressed[i] = true;
				}
				else
				{
					session->MenuWasPressed[i] = false;
				}

				if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
					buttons |= isLeft ? ovrButton_LShoulder : ovrButton_RShoulder;

				if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip))
					inputState->HandTrigger[i] = 1.0f;

				if (state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
					touches |= isLeft ? ovrTouch_LThumb : ovrTouch_RThumb;

				if (state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
					touches |= isLeft ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger;

				// Convert the axes
				for (int j = 0; j < vr::k_unControllerStateAxisCount; j++)
				{
					vr::ETrackedDeviceProperty prop = (vr::ETrackedDeviceProperty)(vr::Prop_Axis0Type_Int32 + j);
					vr::EVRControllerAxisType type = (vr::EVRControllerAxisType)vr::VRSystem()->GetInt32TrackedDeviceProperty(hands[i], prop);
					vr::VRControllerAxis_t axis = state.rAxis[j];

					if (type == vr::k_eControllerAxis_TrackPad)
					{
						if (session->ThumbStick[i])
						{
							// Map the touchpad to the thumbstick with a slightly smaller range
							float x = axis.x / session->ThumbStickRange;
							float y = axis.y / session->ThumbStickRange;
							if (x > 1.0f) x = 1.0f;
							if (y > 1.0f) y = 1.0f;

							inputState->Thumbstick[i].x = x;
							inputState->Thumbstick[i].y = y;
						}

						if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
						{
							if (session->ThumbStick[i])
							{
								buttons |= isLeft ? ovrButton_LThumb : ovrButton_RThumb;
							}
							else
							{
								if (axis.y < axis.x) {
									if (axis.y < -axis.x)
										buttons |= ovrButton_A;
									else
										buttons |= ovrButton_B;
								}
								else {
									if (axis.y < -axis.x)
										buttons |= ovrButton_X;
									else
										buttons |= ovrButton_Y;
								}
							}
						}
					}

					if (type == vr::k_eControllerAxis_Trigger)
						inputState->IndexTrigger[i] = axis.x;
				}

				// Commit buttons/touches, count pressed buttons as touches.
				inputState->Buttons |= buttons;
				inputState->Touches |= touches | buttons;
			}

			inputState->ControllerType = ovrControllerType_Touch;
		}
	}

	if (controllerType & ovrControllerType_Remote)
	{
		// Emulate the remote if we have only one connected controller
		if (controllerCount == 1)
		{
			for (int i = 0; i < controllerCount; i++)
			{
				if (controllers[i] == vr::k_unTrackedDeviceIndexInvalid)
					continue;

				vr::VRControllerState_t state;
				vr::VRSystem()->GetControllerState(controllers[i], &state);

				if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
					inputState->Buttons |= ovrButton_Back;

				// Convert the axes
				for (int j = 0; j < vr::k_unControllerStateAxisCount; j++)
				{
					vr::ETrackedDeviceProperty prop = (vr::ETrackedDeviceProperty)(vr::Prop_Axis0Type_Int32 + j);
					vr::EVRControllerAxisType type = (vr::EVRControllerAxisType)vr::VRSystem()->GetInt32TrackedDeviceProperty(controllers[i], prop);
					vr::VRControllerAxis_t axis = state.rAxis[j];

					if (type == vr::k_eControllerAxis_TrackPad)
					{
						if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
						{
							float magnitude = sqrt(axis.x*axis.x + axis.y*axis.y);

							if (magnitude < 0.5f)
							{
								inputState->Buttons |= ovrButton_Enter;
							}
							else
							{
								if (axis.y < axis.x) {
									if (axis.y < -axis.x)
										inputState->Buttons |= ovrButton_Down;
									else
										inputState->Buttons |= ovrButton_Right;
								}
								else {
									if (axis.y < -axis.x)
										inputState->Buttons |= ovrButton_Left;
									else
										inputState->Buttons |= ovrButton_Up;
								}
							}
						}
					}
				}

				inputState->ControllerType = ovrControllerType_Remote;
			}
		}
	}

	if (controllerType & ovrControllerType_XBox)
	{
		// Use XInput for Xbox controllers.
		XINPUT_STATE state;
		if (g_pXInputGetState(0, &state) == ERROR_SUCCESS)
		{
			// Convert the buttons
			WORD buttons = state.Gamepad.wButtons;
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

			// Set the controller as connected.
			inputState->ControllerType = ovrControllerType_XBox;
		}
	}

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetConnectedControllerTypes(ovrSession session)
{
	REV_TRACE(ovr_GetConnectedControllerTypes);

	unsigned int types = 0;

	// Check for Vive controllers
	uint32_t controllerCount = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_Controller, nullptr, 0);

	// If both controllers are available, it's a touch controller. If not, it's a remote.
	if (controllerCount > 1)
		types |= ovrControllerType_Touch;
	else
		types |= ovrControllerType_Remote;

	// Check for Xbox controller
	XINPUT_STATE input;
	if (g_pXInputGetState(0, &input) == ERROR_SUCCESS)
	{
		types |= ovrControllerType_XBox;
	}

	return types;
}


OVR_PUBLIC_FUNCTION(ovrTouchHapticsDesc) ovr_GetTouchHapticsDesc(ovrSession session, ovrControllerType controllerType)
{
	REV_TRACE(ovr_GetTouchHapticsDesc);
	REV_UNIMPLEMENTED_STRUCT(ovrTouchHapticsDesc);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	REV_TRACE(ovr_SetControllerVibration);

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

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitControllerVibration(ovrSession session, ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	REV_TRACE(ovr_SubmitControllerVibration);
	REV_UNIMPLEMENTED_RUNTIME;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState)
{
	REV_TRACE(ovr_GetControllerVibrationState);
	REV_UNIMPLEMENTED_RUNTIME;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainLength(ovrSession session, ovrTextureSwapChain chain, int* out_Length)
{
	REV_TRACE(ovr_GetTextureSwapChainLength);

	if (!chain)
		return ovrError_InvalidParameter;

	*out_Length = chain->Length;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainCurrentIndex(ovrSession session, ovrTextureSwapChain chain, int* out_Index)
{
	REV_TRACE(ovr_GetTextureSwapChainCurrentIndex);

	if (!chain)
		return ovrError_InvalidParameter;

	*out_Index = chain->CurrentIndex;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainDesc(ovrSession session, ovrTextureSwapChain chain, ovrTextureSwapChainDesc* out_Desc)
{
	REV_TRACE(ovr_GetTextureSwapChainDesc);

	if (!chain)
		return ovrError_InvalidParameter;

	*out_Desc = chain->Desc;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CommitTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	REV_TRACE(ovr_CommitTextureSwapChain);

	if (!chain)
		return ovrError_InvalidParameter;

	chain->Submitted = &chain->Textures[chain->CurrentIndex];
	chain->CurrentIndex++;
	chain->CurrentIndex %= chain->Length;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	REV_TRACE(ovr_DestroyTextureSwapChain);

	if (!chain)
		return;

	vr::VROverlay()->DestroyOverlay(chain->Overlay);
	delete chain;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyMirrorTexture(ovrSession session, ovrMirrorTexture mirrorTexture)
{
	REV_TRACE(ovr_DestroyMirrorTexture);

	if (!mirrorTexture)
		return;

	session->MirrorTexture = nullptr;
	delete mirrorTexture;
}

OVR_PUBLIC_FUNCTION(ovrSizei) ovr_GetFovTextureSize(ovrSession session, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel)
{
	REV_TRACE(ovr_GetFovTextureSize);

	ovrSizei size;
	vr::VRSystem()->GetRecommendedRenderTargetSize((uint32_t*)&size.w, (uint32_t*)&size.h);

	float left, right, top, bottom;
	vr::VRSystem()->GetProjectionRaw((vr::EVREye)eye, &left, &right, &top, &bottom);

	float uMin = 0.5f + 0.5f * left / fov.LeftTan;
	float uMax = 0.5f + 0.5f * right / fov.RightTan;
	float vMin = 0.5f - 0.5f * bottom / fov.UpTan;
	float vMax = 0.5f - 0.5f * top / fov.DownTan;

	// Grow the recommended size to account for the overlapping fov
	size.w = int((size.w * pixelsPerDisplayPixel) / (uMax - uMin));
	size.h = int((size.h * pixelsPerDisplayPixel) / (vMax - vMin));

	return size;
}

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovr_GetRenderDesc(ovrSession session, ovrEyeType eyeType, ovrFovPort fov)
{
	REV_TRACE(ovr_GetRenderDesc);

	ovrEyeRenderDesc desc;
	desc.Eye = eyeType;
	desc.Fov = fov;

	OVR::Matrix4f HmdToEyeMatrix = rev_HmdMatrixToOVRMatrix(vr::VRSystem()->GetEyeToHeadTransform((vr::EVREye)eyeType));
	float WidthTan = fov.LeftTan + fov.RightTan;
	float HeightTan = fov.UpTan + fov.DownTan;
	ovrSizei size;
	vr::VRSystem()->GetRecommendedRenderTargetSize((uint32_t*)&size.w, (uint32_t*)&size.h);

	desc.DistortedViewport = OVR::Recti(eyeType == ovrEye_Right ? size.w : 0, 0, size.w, size.h);
	desc.PixelsPerTanAngleAtCenter = OVR::Vector2f(size.w / WidthTan, size.h / HeightTan);
	desc.HmdToEyeOffset = HmdToEyeMatrix.GetTranslation();

	return desc;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	REV_TRACE(ovr_SubmitFrame);

	// TODO: Implement scaling through ApplyTransform().

	if (!session)
		return ovrError_InvalidSession;

	if (layerCount == 0 || !layerPtrList)
		return ovrError_InvalidParameter;

	// Other layers are interpreted as overlays.
	std::vector<vr::VROverlayHandle_t> activeOverlays;
	for (size_t i = 0; i < layerCount; i++)
	{
		if (layerPtrList[i] == nullptr)
			continue;

		// Overlays are assumed to be monoscopic quads.
		// TODO: Support stereoscopic layers, or at least display them as monoscopic layers.
		if (layerPtrList[i]->Type == ovrLayerType_Quad)
		{
			ovrLayerQuad* layer = (ovrLayerQuad*)layerPtrList[i];

			// Every overlay is associated with a swapchain.
			// This is necessary because the position of the layer may change in the array,
			// which would otherwise cause flickering between overlays.
			vr::VROverlayHandle_t overlay = layer->ColorTexture->Overlay;
			if (overlay == vr::k_ulOverlayHandleInvalid)
			{
				overlay = rev_CreateOverlay(session);
				layer->ColorTexture->Overlay = overlay;
			}
			activeOverlays.push_back(overlay);

			// Set the high quality overlay.
			// FIXME: Why are High quality overlays headlocked in OpenVR?
			//if (layer->Header.Flags & ovrLayerFlag_HighQuality)
			//	vr::VROverlay()->SetHighQualityOverlay(overlay);

			// Add a depth bias to the pose based on the layer order.
			// TODO: Account for the orientation.
			ovrPosef pose = layer->QuadPoseCenter;
			pose.Position.z += (float)i * REV_LAYER_BIAS;

			// Transform the overlay.
			vr::HmdMatrix34_t transform = rev_OvrPoseToHmdMatrix(pose);
			vr::VROverlay()->SetOverlayWidthInMeters(overlay, layer->QuadSize.x);
			if (layer->Header.Flags & ovrLayerFlag_HeadLocked)
				vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(overlay, vr::k_unTrackedDeviceIndex_Hmd, &transform);
			else
				vr::VROverlay()->SetOverlayTransformAbsolute(overlay, vr::VRCompositor()->GetTrackingSpace(), &transform);

			// Set the texture and show the overlay.
			vr::VRTextureBounds_t bounds = rev_ViewportToTextureBounds(layer->Viewport, layer->ColorTexture, layer->Header.Flags);
			vr::VROverlay()->SetOverlayTextureBounds(overlay, &bounds);
			vr::VROverlay()->SetOverlayTexture(overlay, layer->ColorTexture->Submitted);

			// Show the overlay, unfortunately we have no control over the order in which
			// overlays are drawn.
			// TODO: Handle overlay errors.
			vr::VROverlay()->ShowOverlay(overlay);
		}
	}

	// Hide previous overlays that are not part of the current layers.
	for (vr::VROverlayHandle_t overlay : session->ActiveOverlays)
	{
		// Find the overlay in the current active overlays, if it was not found then hide it.
		// TODO: Handle overlay errors.
		if (std::find(activeOverlays.begin(), activeOverlays.end(), overlay) == activeOverlays.end())
			vr::VROverlay()->HideOverlay(overlay);
	}
	session->ActiveOverlays = activeOverlays;

	// The first layer is assumed to be the application scene.
	vr::EVRCompositorError err = vr::VRCompositorError_None;
	if (layerPtrList[0]->Type == ovrLayerType_EyeFov)
	{
		ovrLayerEyeFov* sceneLayer = (ovrLayerEyeFov*)layerPtrList[0];

		// Submit the scene layer.
		for (int i = 0; i < ovrEye_Count; i++)
		{
			ovrTextureSwapChain chain = sceneLayer->ColorTexture[i];
			vr::VRTextureBounds_t bounds = rev_ViewportToTextureBounds(sceneLayer->Viewport[i], sceneLayer->ColorTexture[i], sceneLayer->Header.Flags);

			float left, right, top, bottom;
			vr::VRSystem()->GetProjectionRaw((vr::EVREye)i, &left, &right, &top, &bottom);

			// Shrink the bounds to account for the overlapping fov
			ovrFovPort fov = sceneLayer->Fov[i];
			float uMin = 0.5f + 0.5f * left / fov.LeftTan;
			float uMax = 0.5f + 0.5f * right / fov.RightTan;
			float vMin = 0.5f - 0.5f * bottom / fov.UpTan;
			float vMax = 0.5f - 0.5f * top / fov.DownTan;

			// Combine the fov bounds with the viewport bounds
			bounds.uMin += uMin * bounds.uMax;
			bounds.uMax *= uMax;
			bounds.vMin += vMin * bounds.vMax;
			bounds.vMax *= vMax;

			if (chain->Textures[i].eType == vr::API_OpenGL)
			{
				bounds.vMin = 1.0f - bounds.vMin;
				bounds.vMax = 1.0f - bounds.vMax;
			}

			err = vr::VRCompositor()->Submit((vr::EVREye)i, chain->Submitted, &bounds);
			if (err != vr::VRCompositorError_None)
				break;
		}
	}
	else if (layerPtrList[0]->Type == ovrLayerType_EyeMatrix)
	{
		ovrLayerEyeMatrix* sceneLayer = (ovrLayerEyeMatrix*)layerPtrList[0];

		// Submit the scene layer.
		for (int i = 0; i < ovrEye_Count; i++)
		{
			ovrTextureSwapChain chain = sceneLayer->ColorTexture[i];
			vr::VRTextureBounds_t bounds = rev_ViewportToTextureBounds(sceneLayer->Viewport[i], sceneLayer->ColorTexture[i], sceneLayer->Header.Flags);

			float left, right, top, bottom;
			vr::VRSystem()->GetProjectionRaw((vr::EVREye)i, &left, &right, &top, &bottom);

			// Shrink the bounds to account for the overlapping fov
			ovrVector2f fov = { .5f / sceneLayer->Matrix[i].M[0][0], .5f / sceneLayer->Matrix[i].M[1][1] };
			float uMin = 0.5f + 0.5f * left / fov.x;
			float uMax = 0.5f + 0.5f * right / fov.x;
			float vMin = 0.5f - 0.5f * bottom / fov.y;
			float vMax = 0.5f - 0.5f * top / fov.y;

			// Combine the fov bounds with the viewport bounds
			bounds.uMin += uMin * bounds.uMax;
			bounds.uMax *= uMax;
			bounds.vMin += vMin * bounds.vMax;
			bounds.vMax *= vMax;

			if (chain->Textures[i].eType == vr::API_OpenGL)
			{
				bounds.vMin = 1.0f - bounds.vMin;
				bounds.vMax = 1.0f - bounds.vMax;
			}

			err = vr::VRCompositor()->Submit((vr::EVREye)i, chain->Submitted, &bounds);
			if (err != vr::VRCompositorError_None)
				break;
		}
	}

	// Call WaitGetPoses() to do some cleanup from the previous frame.
	vr::VRCompositor()->WaitGetPoses(session->Poses, vr::k_unMaxTrackedDeviceCount, session->GamePoses, vr::k_unMaxTrackedDeviceCount);

	// Increment the frame index.
	if (frameIndex == 0)
		session->FrameIndex++;
	else
		session->FrameIndex = frameIndex;

	// TODO: Render to the mirror texture here.
	// Currently the mirror texture code is not stable enough yet.

	return rev_CompositorErrorToOvrError(err);
}

OVR_PUBLIC_FUNCTION(double) ovr_GetPredictedDisplayTime(ovrSession session, long long frameIndex)
{
	REV_TRACE(ovr_GetPredictedDisplayTime);

	if (!session)
		return ovrError_InvalidSession;

	float fDisplayFrequency = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
	float fVsyncToPhotons = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);

	// Get the frame count and advance it based on which frame we're predicting
	float fSecondsSinceLastVsync;
	uint64_t unFrame;
	vr::VRSystem()->GetTimeSinceLastVsync(&fSecondsSinceLastVsync, &unFrame);
	unFrame += frameIndex - session->FrameIndex;

	// Predict the display time based on the display frequency and the vsync-to-photon latency
	return double(unFrame) / fDisplayFrequency + fVsyncToPhotons;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds()
{
	REV_TRACE(ovr_GetTimeInSeconds);

	float fDisplayFrequency = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

	// Get the frame count and the time since the last VSync
	float fSecondsSinceLastVsync;
	uint64_t unFrame;
	vr::VRSystem()->GetTimeSinceLastVsync(&fSecondsSinceLastVsync, &unFrame);

	// Calculate the time since the first frame based on the display frequency
	return double(unFrame) / fDisplayFrequency + fSecondsSinceLastVsync;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_GetBool(ovrSession session, const char* propertyName, ovrBool defaultVal)
{
	REV_TRACE(ovr_GetBool);

	if (!session)
		return ovrFalse;

	return vr::VRSettings()->GetBool(REV_SETTINGS_SECTION, propertyName, !!defaultVal);
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetBool(ovrSession session, const char* propertyName, ovrBool value)
{
	REV_TRACE(ovr_SetBool);

	if (!session)
		return ovrFalse;

	vr::EVRSettingsError error;
	vr::VRSettings()->SetBool(REV_SETTINGS_SECTION, propertyName, !!value, &error);
	vr::VRSettings()->Sync();
	return error == vr::VRSettingsError_None;
}

OVR_PUBLIC_FUNCTION(int) ovr_GetInt(ovrSession session, const char* propertyName, int defaultVal)
{
	REV_TRACE(ovr_GetInt);

	if (!session)
		return 0;

	if (strcmp("TextureSwapChainDepth", propertyName) == 0)
		return REV_SWAPCHAIN_LENGTH;

	return vr::VRSettings()->GetInt32(REV_SETTINGS_SECTION, propertyName, defaultVal);
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetInt(ovrSession session, const char* propertyName, int value)
{
	REV_TRACE(ovr_SetInt);

	if (!session)
		return ovrFalse;

	vr::EVRSettingsError error;
	vr::VRSettings()->SetInt32(REV_SETTINGS_SECTION, propertyName, value, &error);
	vr::VRSettings()->Sync();
	return error == vr::VRSettingsError_None;
}

OVR_PUBLIC_FUNCTION(float) ovr_GetFloat(ovrSession session, const char* propertyName, float defaultVal)
{
	REV_TRACE(ovr_GetFloat);

	if (!session)
		return 0.0f;

	if (strcmp(propertyName, "IPD") == 0)
		return vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserIpdMeters_Float);

	// Override defaults, we should always return a valid value for these
	if (strcmp(propertyName, OVR_KEY_PLAYER_HEIGHT) == 0)
		defaultVal = OVR_DEFAULT_PLAYER_HEIGHT;
	else if (strcmp(propertyName, OVR_KEY_EYE_HEIGHT) == 0)
		defaultVal = OVR_DEFAULT_EYE_HEIGHT;

	return vr::VRSettings()->GetFloat(REV_SETTINGS_SECTION, propertyName, defaultVal);
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloat(ovrSession session, const char* propertyName, float value)
{
	REV_TRACE(ovr_SetFloat);

	if (!session)
		return ovrFalse;

	vr::EVRSettingsError error;
	vr::VRSettings()->SetFloat(REV_SETTINGS_SECTION, propertyName, value, &error);
	vr::VRSettings()->Sync();
	return error == vr::VRSettingsError_None;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetFloatArray(ovrSession session, const char* propertyName, float values[], unsigned int valuesCapacity)
{
	REV_TRACE(ovr_GetFloatArray);

	if (!session)
		return 0;

	if (strcmp(propertyName, OVR_KEY_NECK_TO_EYE_DISTANCE) == 0)
	{
		if (valuesCapacity < 2)
			return 0;

		// We only know the horizontal depth
		values[0] = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserHeadToEyeDepthMeters_Float);
		values[1] = OVR_DEFAULT_NECK_TO_EYE_VERTICAL;
		return 2;
	}

	char key[vr::k_unMaxSettingsKeyLength] = { 0 };

	for (size_t i = 0; i < valuesCapacity; i++)
	{
		vr::EVRSettingsError error;
		snprintf(key, vr::k_unMaxSettingsKeyLength, "%s[%d]", propertyName, i);
		values[i] = vr::VRSettings()->GetFloat(REV_SETTINGS_SECTION, key, 0.0f, &error);

		if (error != vr::VRSettingsError_None)
			return i;
	}

	return valuesCapacity;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloatArray(ovrSession session, const char* propertyName, const float values[], unsigned int valuesSize)
{
	REV_TRACE(ovr_SetFloatArray);

	if (!session)
		return ovrFalse;

	char key[vr::k_unMaxSettingsKeyLength] = { 0 };

	for (size_t i = 0; i < valuesSize; i++)
	{
		vr::EVRSettingsError error;
		snprintf(key, vr::k_unMaxSettingsKeyLength, "%s[%d]", propertyName, i);
		vr::VRSettings()->SetFloat(REV_SETTINGS_SECTION, key, values[i], &error);

		if (error != vr::VRSettingsError_None)
			return false;
	}

	vr::VRSettings()->Sync();
	return true;
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetString(ovrSession session, const char* propertyName, const char* defaultVal)
{
	REV_TRACE(ovr_GetString);

	if (!session)
		return nullptr;

	// Override defaults, we should always return a valid value for these
	if (strcmp(propertyName, OVR_KEY_GENDER) == 0)
		defaultVal = OVR_DEFAULT_GENDER;

	vr::VRSettings()->GetString(REV_SETTINGS_SECTION, propertyName, session->StringBuffer, vr::k_unMaxPropertyStringSize, defaultVal);
	return session->StringBuffer;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetString(ovrSession session, const char* propertyName, const char* value)
{
	REV_TRACE(ovr_SetString);

	if (!session)
		return ovrFalse;

	vr::EVRSettingsError error;
	vr::VRSettings()->SetString(REV_SETTINGS_SECTION, propertyName, value, &error);
	vr::VRSettings()->Sync();
	return error == vr::VRSettingsError_None;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Lookup(const char* name, void** data)
{
	REV_UNIMPLEMENTED_RUNTIME;
}
