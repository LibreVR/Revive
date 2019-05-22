#include "InputManager.h"
#include "Session.h"
#include "SessionDetails.h"
#include "Settings.h"
#include "SettingsManager.h"
#include "CompositorBase.h"
#include "OVR_CAPI.h"
#include "REV_Math.h"
#include "rcu_ptr.h"

#include <openvr.h>
#include <algorithm>
#include <Windows.h>
#include <Shlobj.h>
#include <atlbase.h>

InputManager::InputManager()
	: m_InputDevices()
	, m_LastPoses()
{
	for (ovrPoseStatef& pose : m_LastPoses)
		pose.ThePose = OVR::Posef::Identity();

	// TODO: This might change if a new HMD is connected (unlikely)
	m_fVsyncToPhotons = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);

	LoadActionManifest();

	vr::VRActionSetHandle_t handle;
	vr::EVRInputError err = vr::VRInput()->GetActionSetHandle("/actions/xbox", &handle);
	if (err == vr::VRInputError_None)
		m_InputDevices.push_back(new XboxGamepad(handle));
	err = vr::VRInput()->GetActionSetHandle("/actions/remote", &handle);
	if (err == vr::VRInputError_None)
		m_InputDevices.push_back(new OculusRemote(handle));
	err = vr::VRInput()->GetActionSetHandle("/actions/touch", &handle);
	if (err == vr::VRInputError_None)
	{
		m_InputDevices.push_back(new OculusTouch(handle, vr::TrackedControllerRole_LeftHand));
		m_InputDevices.push_back(new OculusTouch(handle, vr::TrackedControllerRole_RightHand));
	}

	UpdateConnectedControllers();
}

InputManager::~InputManager()
{
	for (InputDevice* device : m_InputDevices)
		delete device;
}

void InputManager::LoadActionManifest()
{
	CComHeapPtr<wchar_t> folder;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, NULL, &folder);

	if (SUCCEEDED(hr))
	{
		char path[MAX_PATH];
		WideCharToMultiByte(CP_ACP, 0, folder, -1, path, MAX_PATH, nullptr, nullptr);
		strcat(path, "\\Revive\\Input\\action_manifest.json");
		vr::EVRInputError err = vr::VRInput()->SetActionManifestPath(path);
		if (err == vr::VRInputError_None)
			return;
	}

	vr::VROverlay()->ShowMessageOverlay("Failed to load action manifest, input will not function correctly!", "Action manifest error", "Continue");
}

void InputManager::UpdateConnectedControllers()
{
	uint32_t types = 0;
	for (InputDevice* device : m_InputDevices)
	{
		if (device->IsConnected())
			types |= device->GetType();
	}
	ConnectedControllers = types;
}

void InputManager::UpdateInputState()
{
	UpdateConnectedControllers();

	std::vector<vr::VRActiveActionSet_t> sets;
	for (InputDevice* device : m_InputDevices)
	{
		vr::VRActiveActionSet_t set;
		set.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
		set.ulActionSet = device->ActionSet;
		sets.push_back(set);
	}

	vr::VRInput()->UpdateActionState(sets.data(), sizeof(vr::VRActiveActionSet_t), (uint32_t)sets.size());
}

ovrResult InputManager::SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	// Clamp the input
	frequency = std::min(std::max(frequency, 0.0f), 1.0f);
	amplitude = std::min(std::max(amplitude, 0.0f), 1.0f);

	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && ConnectedControllers & device->GetType())
			device->SetVibration(frequency, amplitude);
	}

	return ovrSuccess;
}

ovrResult InputManager::GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	memset(inputState, 0, sizeof(ovrInputState));

	uint32_t types = 0;
	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && ConnectedControllers & device->GetType())
		{
			if (device->GetInputState(session, inputState))
				types |= device->GetType();
		}
	}

	inputState->TimeInSeconds = ovr_GetTimeInSeconds();
	inputState->ControllerType = (ovrControllerType)types;
	return ovrSuccess;
}

ovrResult InputManager::SubmitControllerVibration(ovrSession session, ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && ConnectedControllers & device->GetType())
			device->SubmitVibration(buffer);
	}

	return ovrSuccess;
}

ovrResult InputManager::GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState)
{
	memset(outState, 0, sizeof(ovrHapticsPlaybackState));

	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && ConnectedControllers & device->GetType())
			device->GetVibrationState(outState);
	}

	return ovrSuccess;
}

ovrTouchHapticsDesc InputManager::GetTouchHapticsDesc(ovrControllerType controllerType)
{
	ovrTouchHapticsDesc desc = { 0 };

	if (controllerType & ovrControllerType_Touch)
	{
		desc.SampleRateHz = REV_HAPTICS_SAMPLE_RATE;
		desc.SampleSizeInBytes = sizeof(uint8_t);
		desc.SubmitMaxSamples = OVR_HAPTICS_BUFFER_SAMPLES_MAX;
		desc.SubmitMinSamples = 1;
		desc.SubmitOptimalSamples = 20;
		desc.QueueMinSizeToAvoidStarvation = 5;
	}

	return desc;
}

unsigned int InputManager::TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose)
{
	unsigned int result = 0;

	if (pose.bPoseIsValid)
	{
		result = ovrStatus_OrientationValid | ovrStatus_PositionValid;
		if (pose.bDeviceIsConnected)
			result |= ovrStatus_OrientationTracked;
		if (pose.eTrackingResult != vr::TrackingResult_Calibrating_OutOfRange &&
			pose.eTrackingResult != vr::TrackingResult_Running_OutOfRange)
			result |= ovrStatus_PositionTracked;
	}

	return result;
}

ovrPoseStatef InputManager::TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, ovrPoseStatef& lastPose, double time)
{
	ovrPoseStatef result = { OVR::Posef::Identity() };
	if (!pose.bPoseIsValid)
		return result;

	OVR::Matrix4f matrix = REV::Matrix4f(pose.mDeviceToAbsoluteTracking);

	// Make sure the orientation stays in the same hemisphere as the previous orientation, this prevents
	// linear interpolations from suddenly flipping the long way around in Oculus Medium.
	OVR::Quatf q(matrix);
	q.EnsureSameHemisphere(lastPose.ThePose.Orientation);

	result.ThePose.Orientation = q;
	result.ThePose.Position = matrix.GetTranslation();
	result.AngularVelocity = (REV::Vector3f)pose.vAngularVelocity;
	result.LinearVelocity = (REV::Vector3f)pose.vVelocity;
	result.AngularAcceleration = ((REV::Vector3f)pose.vAngularVelocity - lastPose.AngularVelocity) / float(time - lastPose.TimeInSeconds);
	result.LinearAcceleration = ((REV::Vector3f)pose.vVelocity - lastPose.LinearVelocity) / float(time - lastPose.TimeInSeconds);
	result.TimeInSeconds = time;

	// Store the last pose
	lastPose = result;

	return result;
}

void InputManager::GetTrackingState(ovrSession session, ovrTrackingState* outState, double absTime)
{
	if (session->Details->UseHack(SessionDetails::HACK_WAIT_IN_TRACKING_STATE))
		vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);

	// Calculate the relative prediction time
	float relTime = 0.0f;
	if (absTime > 0.0f)
		relTime = float(absTime - ovr_GetTimeInSeconds());
	if (relTime > 0.0f)
		relTime += m_fVsyncToPhotons;

	// Get the device poses
	vr::ETrackingUniverseOrigin origin = session->TrackingOrigin;
	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(origin, relTime, poses, vr::k_unMaxTrackedDeviceCount);

	// Convert the head pose
	outState->HeadPose = TrackedDevicePoseToOVRPose(poses[vr::k_unTrackedDeviceIndex_Hmd], m_LastPoses[vr::k_unTrackedDeviceIndex_Hmd], absTime);
	outState->StatusFlags = TrackedDevicePoseToOVRStatusFlags(poses[vr::k_unTrackedDeviceIndex_Hmd]);

	// Convert the hand poses
	rcu_ptr<InputSettings> settings = session->Settings->Input;
	vr::TrackedDeviceIndex_t hands[] = { vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),
		vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) };
	for (int i = 0; i < ovrHand_Count; i++)
	{
		vr::TrackedDeviceIndex_t deviceIndex = hands[i];
		if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid)
		{
			outState->HandPoses[i].ThePose = OVR::Posef::Identity();
			continue;
		}

		vr::TrackedDevicePose_t pose;
		vr::VRSystem()->ApplyTransform(&pose, &poses[deviceIndex], &settings->TouchOffset[i]);
		outState->HandPoses[i] = TrackedDevicePoseToOVRPose(pose, m_LastPoses[deviceIndex], absTime);
		outState->HandStatusFlags[i] = TrackedDevicePoseToOVRStatusFlags(poses[deviceIndex]);
	}

	// TODO: Calibrate the origin ourselves instead of relying on OpenVR.
	outState->CalibratedOrigin.Orientation = OVR::Quatf::Identity();
	outState->CalibratedOrigin.Position = OVR::Vector3f();
}

ovrResult InputManager::GetDevicePoses(ovrTrackedDeviceType* deviceTypes, int deviceCount, double absTime, ovrPoseStatef* outDevicePoses)
{
	// Get the device poses
	vr::ETrackingUniverseOrigin space = vr::VRCompositor()->GetTrackingSpace();
	float relTime = float(absTime - ovr_GetTimeInSeconds());
	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(space, relTime, poses, vr::k_unMaxTrackedDeviceCount);

	// Get the generic tracker indices
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount];
	vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_GenericTracker, trackers, vr::k_unMaxTrackedDeviceCount);

	for (int i = 0; i < deviceCount; i++)
	{
		// Get the index for device types we recognize
		uint32_t index = vr::k_unTrackedDeviceIndexInvalid;
		switch (deviceTypes[i])
		{
		case ovrTrackedDevice_HMD:
			index = vr::k_unTrackedDeviceIndex_Hmd;
			break;
		case ovrTrackedDevice_LTouch:
			index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
			break;
		case ovrTrackedDevice_RTouch:
			index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
			break;
		case ovrTrackedDevice_Object0:
			index = trackers[0];
			break;
		case ovrTrackedDevice_Object1:
			index = trackers[1];
			break;
		case ovrTrackedDevice_Object2:
			index = trackers[2];
			break;
		case ovrTrackedDevice_Object3:
			index = trackers[3];
			break;
		}

		// If the tracking index is invalid it will fall outside of the range of the array
		if (index >= vr::k_unMaxTrackedDeviceCount)
			return ovrError_DeviceUnavailable;
		outDevicePoses[i] = TrackedDevicePoseToOVRPose(poses[index], m_LastPoses[index], absTime);
	}

	return ovrSuccess;
}

/* Controller child-classes */

OVR::Vector2f InputManager::InputDevice::ApplyDeadzone(OVR::Vector2f axis, float deadZoneLow, float deadZoneHigh)
{
	float mag = axis.Length();
	if (mag > deadZoneLow)
	{
		// scale such that output magnitude is in the range[0, 1]
		float legalRange = 1.0f - deadZoneHigh - deadZoneLow;
		float normalizedMag = std::min(1.0f, (mag - deadZoneLow) / legalRange);
		float scale = normalizedMag / mag;
		return axis * scale;
	}
	else
	{
		// stick is in the inner dead zone
		return OVR::Vector2f();
	}
}

void InputManager::OculusTouch::HapticsThread(OculusTouch* device)
{
	std::chrono::microseconds freq(std::chrono::seconds(1));
	freq /= REV_HAPTICS_SAMPLE_RATE;

	while (device->m_bHapticsRunning)
	{
		vr::TrackedDeviceIndex_t touch = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(device->Role);

		uint16_t duration = (uint16_t)((float)freq.count() * device->m_Haptics.GetSample());
		if (duration > 0)
			vr::VRSystem()->TriggerHapticPulse(touch, 0, duration);

		std::this_thread::sleep_for(freq);
	}
}

InputManager::OculusTouch::OculusTouch(vr::VRActionSetHandle_t actionSet, vr::ETrackedControllerRole role)
	: InputDevice(actionSet)
	, Role(role)
	, WasGripped(false)
	, TimeGripped(0.0)
	, m_bHapticsRunning(true)
{
	vr::VRInput()->GetActionHandle("/actions/touch/in/Button_Enter", &m_Button_Enter);

#define GET_HANDED_ACTION(x, r, l) vr::VRInput()->GetActionHandle( \
	role == vr::TrackedControllerRole_RightHand ? "/actions/touch/in/" #r : "/actions/touch/in/" #l, x)

	GET_HANDED_ACTION(&m_Button_AX, Button_A, Button_X);
	GET_HANDED_ACTION(&m_Button_BY, Button_B, Button_Y);
	GET_HANDED_ACTION(&m_Button_Thumb, Button_RThumb, Button_LThumb);

	GET_HANDED_ACTION(&m_Touch_AX, Touch_A, Touch_X);
	GET_HANDED_ACTION(&m_Touch_BY, Touch_B, Touch_Y);
	GET_HANDED_ACTION(&m_Touch_Thumb, Touch_RThumb, Touch_LThumb);
	GET_HANDED_ACTION(&m_Touch_ThumbRest, Touch_RThumbRest, Touch_LThumbRest);
	GET_HANDED_ACTION(&m_Touch_IndexTrigger, Touch_RIndexTrigger, Touch_LIndexTrigger);

	GET_HANDED_ACTION(&m_IndexTrigger, RIndexTrigger, LIndexTrigger);
	GET_HANDED_ACTION(&m_HandTrigger, RHandTrigger, LHandTrigger);
	GET_HANDED_ACTION(&m_Thumbstick, RThumbstick, LThumbstick);
	GET_HANDED_ACTION(&m_Recenter_Thumb, Recenter_RThumb, Recenter_LThumb);

	GET_HANDED_ACTION(&m_Button_IndexTrigger, Button_RIndexTrigger, Button_LIndexTrigger);
	GET_HANDED_ACTION(&m_Button_HandTrigger, Button_RHandTrigger, Button_LHandTrigger);

	GET_HANDED_ACTION(&m_Vibration, RVibration, LVibration);

#undef GET_HANDED_ACTION

	m_HapticsThread = std::thread(HapticsThread, this);
}

InputManager::OculusTouch::~OculusTouch()
{
	m_bHapticsRunning = false;
	m_HapticsThread.join();
}

ovrControllerType InputManager::OculusTouch::GetType()
{
	return (Role == vr::TrackedControllerRole_LeftHand) ? ovrControllerType_LTouch : ovrControllerType_RTouch;
}

bool InputManager::OculusTouch::IsConnected() const
{
	// Check if a Vive controller is available
	uint32_t controllerCount = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_Controller, nullptr, 0);

	// If only one controller is available, the Oculus Remote is connected
	return controllerCount > 1;
}

bool InputManager::OculusTouch::GetInputState(ovrSession session, ovrInputState* inputState)
{
	ovrHandType hand = (Role == vr::TrackedControllerRole_LeftHand) ? ovrHand_Left : ovrHand_Right;

	if (GetDigital(m_Button_Enter))
		inputState->Buttons |= ovrButton_Enter;

	unsigned int buttons = 0, touches = 0;

	if (GetDigital(m_Button_AX))
		buttons |= ovrButton_A;

	if (GetDigital(m_Touch_AX))
		touches |= ovrTouch_A;

	if (GetDigital(m_Button_BY))
		buttons |= ovrButton_B;

	if (GetDigital(m_Touch_BY))
		touches |= ovrTouch_B;

	if (GetDigital(m_Button_Thumb))
		buttons |= ovrButton_RThumb;

	if (GetDigital(m_Touch_Thumb))
		touches |= ovrTouch_RThumb;

	if (GetDigital(m_Touch_ThumbRest))
		touches |= ovrTouch_RThumbRest;

	if (GetDigital(m_Touch_IndexTrigger))
		touches |= ovrTouch_RIndexTrigger;

	// FIXME: Can't use IsReleased or IsPressed, because bChanged resets after every call to GetDigitalActionData
	vr::InputDigitalActionData_t recenterData = {};
	vr::VRInput()->GetDigitalActionData(m_Recenter_Thumb, &recenterData, sizeof(recenterData));
	if (recenterData.bChanged)
	{
		if (recenterData.bState)
			m_Thumbstick_Center = GetAnalog(m_Thumbstick);
		else
			m_Thumbstick_Center = OVR::Vector2f::Zero();
	}

	// FIXME: Can't use IsReleased or IsPressed, because bChanged resets after every call to GetDigitalActionData
	vr::InputDigitalActionData_t triggerData = {};
	vr::VRInput()->GetDigitalActionData(m_Button_HandTrigger, &triggerData, sizeof(triggerData));

	// Read the input settings
	float deadzone;
	{
		rcu_ptr<InputSettings> settings = session->Settings->Input;
		deadzone = settings->Deadzone;

		if (settings->ToggleGrip == revGrip_Hybrid)
		{
			if (triggerData.bChanged)
			{
				if (triggerData.bState)
				{
					// Only set the timestamp on the first grip toggle, we don't want to toggle twice
					if (!WasGripped)
						TimeGripped = ovr_GetTimeInSeconds();

					WasGripped = true;
				}
				else
				{
					if (ovr_GetTimeInSeconds() - TimeGripped > settings->ToggleDelay)
						WasGripped = false;

					// Next time we always want to release grip
					TimeGripped = 0.0;
				}
			}
		}
		else if (settings->ToggleGrip == revGrip_Toggle)
		{
			if (triggerData.bChanged && triggerData.bState)
				WasGripped = !WasGripped;
		}
		else
		{
			WasGripped = GetDigital(m_Button_HandTrigger);
		}
	}

	OVR::Vector2f thumbstick = GetAnalog(m_Thumbstick) - m_Thumbstick_Center;
	inputState->IndexTrigger[hand] = GetAnalog(m_IndexTrigger).x;
	inputState->HandTrigger[hand] = GetAnalog(m_HandTrigger).x;
	inputState->Thumbstick[hand] = ApplyDeadzone(thumbstick, deadzone, deadzone / 2.0f);
	inputState->ThumbstickNoDeadzone[hand] = thumbstick;

	if (GetDigital(m_Button_IndexTrigger))
		inputState->IndexTrigger[hand] = 1.0f;

	if (WasGripped)
		inputState->HandTrigger[hand] = 1.0f;

	// Derive gestures from touch flags
	// TODO: Should be handled with chords in SteamVR input
	if (inputState->HandTrigger[hand] > 0.5f)
	{
		if (!(touches & ovrTouch_RIndexTrigger))
			touches |= ovrTouch_RIndexPointing;

		if (!(touches & ~(ovrTouch_RIndexTrigger | ovrTouch_RIndexPointing)))
			touches |= ovrTouch_RThumbUp;
	}

	// We don't apply deadzones yet on triggers and grips
	inputState->IndexTriggerNoDeadzone[hand] = inputState->IndexTrigger[hand];
	inputState->HandTriggerNoDeadzone[hand] = inputState->HandTrigger[hand];

	// We have no way to get raw values
	inputState->ThumbstickRaw[hand] = inputState->ThumbstickNoDeadzone[hand];
	inputState->IndexTriggerRaw[hand] = inputState->IndexTriggerNoDeadzone[hand];
	inputState->HandTriggerRaw[hand] = inputState->HandTriggerNoDeadzone[hand];

	inputState->Buttons |= (hand == ovrHand_Left) ? buttons << 8 : buttons;
	inputState->Touches |= (hand == ovrHand_Left) ? touches << 8 : touches;

	return true;
}

InputManager::OculusRemote::OculusRemote(vr::VRActionSetHandle_t actionSet)
	: InputDevice(actionSet)
{
#define GET_REMOTE_ACTION(x) vr::VRInput()->GetActionHandle( \
	"/actions/remote/in/" #x, &m_##x)

	GET_REMOTE_ACTION(Button_Up);
	GET_REMOTE_ACTION(Button_Down);
	GET_REMOTE_ACTION(Button_Left);
	GET_REMOTE_ACTION(Button_Right);
	GET_REMOTE_ACTION(Button_Enter);
	GET_REMOTE_ACTION(Button_Back);
	GET_REMOTE_ACTION(Button_VolUp);
	GET_REMOTE_ACTION(Button_VolDown);

#undef GET_REMOTE_ACTION
}

bool InputManager::OculusRemote::IsConnected() const
{
	// Check if a Vive controller is available
	uint32_t controllerCount = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_Controller, nullptr, 0);

	// If only one controller is available, the Oculus Remote is connected
	return controllerCount == 1;
}

bool InputManager::OculusRemote::GetInputState(ovrSession session, ovrInputState* inputState)
{
	unsigned int buttons;

	if (GetDigital(m_Button_Up))
		buttons |= ovrButton_Up;

	if (GetDigital(m_Button_Down))
		buttons |= ovrButton_Down;

	if (GetDigital(m_Button_Left))
		buttons |= ovrButton_Left;

	if (GetDigital(m_Button_Right))
		buttons |= ovrButton_Right;

	if (GetDigital(m_Button_Enter))
		buttons |= ovrButton_Enter;

	if (GetDigital(m_Button_Back))
		buttons |= ovrButton_Back;

	if (GetDigital(m_Button_VolUp))
		buttons |= ovrButton_VolUp;

	if (GetDigital(m_Button_VolDown))
		buttons |= ovrButton_VolDown;

	inputState->Buttons |= buttons;
	return true;
}

InputManager::XboxGamepad::XboxGamepad(vr::VRActionSetHandle_t actionSet)
	: InputDevice(actionSet)
{
#define GET_XBOX_ACTION(x) vr::VRInput()->GetActionHandle( \
	"/actions/xbox/in/" #x, &m_##x)

	GET_XBOX_ACTION(Button_A);
	GET_XBOX_ACTION(Button_B);
	GET_XBOX_ACTION(Button_RThumb);
	GET_XBOX_ACTION(Button_RShoulder);
	GET_XBOX_ACTION(Button_X);
	GET_XBOX_ACTION(Button_Y);
	GET_XBOX_ACTION(Button_LThumb);
	GET_XBOX_ACTION(Button_LShoulder);
	GET_XBOX_ACTION(Button_Up);
	GET_XBOX_ACTION(Button_Down);
	GET_XBOX_ACTION(Button_Left);
	GET_XBOX_ACTION(Button_Right);
	GET_XBOX_ACTION(Button_Enter);
	GET_XBOX_ACTION(Button_Back);
	GET_XBOX_ACTION(RIndexTrigger);
	GET_XBOX_ACTION(LIndexTrigger);
	GET_XBOX_ACTION(RThumbstick);
	GET_XBOX_ACTION(LThumbstick);

#undef GET_XBOX_ACTION
}

InputManager::XboxGamepad::~XboxGamepad()
{
}

bool InputManager::XboxGamepad::GetInputState(ovrSession session, ovrInputState* inputState)
{
	unsigned int buttons = 0;

	if (GetDigital(m_Button_A))
		buttons |= ovrButton_A;

	if (GetDigital(m_Button_B))
		buttons |= ovrButton_B;

	if (GetDigital(m_Button_RThumb))
		buttons |= ovrButton_RThumb;

	if (GetDigital(m_Button_RShoulder))
		buttons |= ovrButton_RShoulder;

	if (GetDigital(m_Button_X))
		buttons |= ovrButton_X;

	if (GetDigital(m_Button_Y))
		buttons |= ovrButton_Y;

	if (GetDigital(m_Button_LThumb))
		buttons |= ovrButton_LThumb;

	if (GetDigital(m_Button_LShoulder))
		buttons |= ovrButton_LShoulder;

	if (GetDigital(m_Button_Up))
		buttons |= ovrButton_Up;

	if (GetDigital(m_Button_Down))
		buttons |= ovrButton_Down;

	if (GetDigital(m_Button_Left))
		buttons |= ovrButton_Left;

	if (GetDigital(m_Button_Right))
		buttons |= ovrButton_Right;

	if (GetDigital(m_Button_Enter))
		buttons |= ovrButton_Enter;

	if (GetDigital(m_Button_Back))
		buttons |= ovrButton_Back;

	vr::VRActionHandle_t triggers[] = { m_LIndexTrigger, m_RIndexTrigger };
	vr::VRActionHandle_t sticks[] = { m_LThumbstick, m_RThumbstick };
	float deadzone[] = { 0.24f, 0.265f };
	for (int hand = 0; hand < ovrHand_Count; hand++)
	{
		OVR::Vector2f thumbstick = GetAnalog(sticks[hand]);
		inputState->IndexTrigger[hand] = GetAnalog(triggers[hand]).x;
		inputState->Thumbstick[hand] = ApplyDeadzone(thumbstick, deadzone[hand], deadzone[hand] / 2.0f);
		inputState->ThumbstickNoDeadzone[hand] = thumbstick;

		// We don't apply deadzones yet on triggers and grips
		inputState->IndexTriggerNoDeadzone[hand] = inputState->IndexTrigger[hand];
		inputState->HandTriggerNoDeadzone[hand] = inputState->HandTrigger[hand];

		// We have no way to get raw values
		inputState->ThumbstickRaw[hand] = inputState->ThumbstickNoDeadzone[hand];
		inputState->IndexTriggerRaw[hand] = inputState->IndexTriggerNoDeadzone[hand];
		inputState->HandTriggerRaw[hand] = inputState->HandTriggerNoDeadzone[hand];
	}

	return true;
}

void InputManager::XboxGamepad::SetVibration(float frequency, float amplitude)
{
}
