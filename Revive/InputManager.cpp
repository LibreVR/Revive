#include "InputManager.h"
#include "OVR_CAPI.h"
#include "Common.h"

#include <openvr.h>
#include <Windows.h>
#include <Xinput.h>

InputManager::InputManager()
{
	m_InputDevices.push_back(new XboxGamepad());
	m_InputDevices.push_back(new OculusTouch(vr::TrackedControllerRole_LeftHand));
	m_InputDevices.push_back(new OculusTouch(vr::TrackedControllerRole_RightHand));
	m_InputDevices.push_back(new OculusRemote());
}

InputManager::~InputManager()
{
	for (InputDevice* device : m_InputDevices)
		delete device;
}

unsigned int InputManager::GetConnectedControllerTypes()
{
	uint32_t types = 0;
	for (InputDevice* device : m_InputDevices)
	{
		if (device->IsConnected())
			types |= device->GetType();
	}

	return types;
}

ovrResult InputManager::SetControllerVibration(ovrControllerType controllerType, float frequency, float amplitude)
{
	// Clamp the input
	frequency = min(max(frequency, 0.0f), 1.0f);
	amplitude = min(max(amplitude, 0.0f), 1.0f);

	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
			device->SetVibration(frequency, amplitude);
	}

	return ovrSuccess;
}

ovrResult InputManager::GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	memset(inputState, 0, sizeof(ovrInputState));

	inputState->TimeInSeconds = ovr_GetTimeInSeconds();

	uint32_t types = 0;
	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
		{
			if (device->GetInputState(session, inputState))
				types |= device->GetType();
		}
	}

	inputState->ControllerType = (ovrControllerType)types;
	return ovrSuccess;
}

ovrResult InputManager::SubmitControllerVibration(ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
			device->SubmitVibration(buffer);
	}

	return ovrSuccess;
}

ovrResult InputManager::GetControllerVibrationState(ovrControllerType controllerType, ovrHapticsPlaybackState* outState)
{
	memset(outState, 0, sizeof(ovrHapticsPlaybackState));

	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
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
		desc.SubmitMaxSamples = REV_HAPTICS_MAX_SAMPLES;
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
		if (pose.bDeviceIsConnected)
			result |= ovrStatus_OrientationTracked;
		if (pose.eTrackingResult != vr::TrackingResult_Calibrating_OutOfRange &&
			pose.eTrackingResult != vr::TrackingResult_Running_OutOfRange)
			result |= ovrStatus_PositionTracked;
	}

	return result;
}

ovrPoseStatef InputManager::TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, double time)
{
	ovrPoseStatef result = { 0 };
	result.ThePose = OVR::Posef::Identity();

	OVR::Matrix4f matrix;
	if (pose.bPoseIsValid)
		matrix = rev_HmdMatrixToOVRMatrix(pose.mDeviceToAbsoluteTracking);
	else
		return result;

	result.ThePose.Orientation = OVR::Quatf(matrix);
	result.ThePose.Position = matrix.GetTranslation();
	result.AngularVelocity = rev_HmdVectorToOVRVector(pose.vAngularVelocity);
	result.LinearVelocity = rev_HmdVectorToOVRVector(pose.vVelocity);
	// TODO: Calculate acceleration.
	result.AngularAcceleration = ovrVector3f();
	result.LinearAcceleration = ovrVector3f();
	result.TimeInSeconds = time;

	return result;
}

void InputManager::GetTrackingState(ovrSession session, ovrTrackingState* outState, double absTime)
{
	// Get the device poses
	vr::ETrackingUniverseOrigin space = vr::VRCompositor()->GetTrackingSpace();
	float relTime = float(absTime - ovr_GetTimeInSeconds());
	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	if (session->Details->UseHack(SessionDetails::HACK_WAIT_IN_TRACKING_STATE))
		vr::VRCompositor()->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
	else
		vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(space, relTime, poses, vr::k_unMaxTrackedDeviceCount);

	// Convert the head pose
	outState->HeadPose = TrackedDevicePoseToOVRPose(poses[vr::k_unTrackedDeviceIndex_Hmd], absTime);
	outState->StatusFlags = TrackedDevicePoseToOVRStatusFlags(poses[vr::k_unTrackedDeviceIndex_Hmd]);

	// Convert the hand poses
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
		vr::VRSystem()->ApplyTransform(&pose, &poses[deviceIndex], &session->TouchOffset[i]);
		outState->HandPoses[i] = TrackedDevicePoseToOVRPose(pose, absTime);
		outState->HandStatusFlags[i] = TrackedDevicePoseToOVRStatusFlags(poses[deviceIndex]);
	}

	if (space == vr::TrackingUniverseSeated)
	{
		OVR::Matrix4f origin = rev_HmdMatrixToOVRMatrix(vr::VRSystem()->GetSeatedZeroPoseToStandingAbsoluteTrackingPose());

		// The calibrated origin should be the location of the seated origin relative to the absolute tracking space.
		// It currently describes the location of the absolute origin relative to the seated origin, so we have to invert it.
		origin.Invert();

		outState->CalibratedOrigin.Orientation = OVR::Quatf(origin);
		outState->CalibratedOrigin.Position = origin.GetTranslation();
	}
	else
	{
		// In a standing universe we don't calibrate the origin outside of the room setup, thus this should always be the
		// identity matrix.
		outState->CalibratedOrigin.Orientation = OVR::Quatf::Identity();
		outState->CalibratedOrigin.Position = OVR::Vector3f();
	}
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
		if (index < vr::k_unMaxTrackedDeviceCount)
			outDevicePoses[i] = TrackedDevicePoseToOVRPose(poses[index], absTime);
		else
			return ovrError_DeviceUnavailable;
	}

	return ovrSuccess;
}

/* Controller child-classes */

void InputManager::OculusTouch::HapticsThread(OculusTouch* device)
{
	std::chrono::microseconds freq(std::chrono::seconds(1));
	freq /= REV_HAPTICS_SAMPLE_RATE;

	while (device->m_bHapticsRunning)
	{
		vr::TrackedDeviceIndex_t touch = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(device->GetRole());

		uint16_t duration = (uint16_t)((float)freq.count() * device->m_Haptics.GetSample());
		if (duration > 0)
			vr::VRSystem()->TriggerHapticPulse(touch, 0, duration);

		std::this_thread::sleep_for(freq);
	}
}

ovrTouch InputManager::OculusTouch::AxisToTouch(vr::VRControllerAxis_t axis)
{
	if (m_Role == vr::TrackedControllerRole_LeftHand)
	{
		if (axis.y < axis.x) {
			if (axis.y < -axis.x)
				return ovrTouch_X;
			else
				return ovrTouch_Y;
		}
		else {
			return ovrTouch_LThumb;
		}
	}
	else
	{
		if (axis.y < -axis.x) {
			if (axis.y < axis.x)
				return ovrTouch_A;
			else
				return ovrTouch_B;
		}
		else {
			return ovrTouch_RThumb;
		}
	}
}

bool InputManager::OculusTouch::IsPressed(vr::VRControllerState_t newState, vr::EVRButtonId button)
{
	return newState.ulButtonPressed & vr::ButtonMaskFromId(button) &&
		!(m_LastState.ulButtonPressed & vr::ButtonMaskFromId(button));
}

bool InputManager::OculusTouch::IsReleased(vr::VRControllerState_t newState, vr::EVRButtonId button)
{
	return !(newState.ulButtonPressed & vr::ButtonMaskFromId(button)) &&
		m_LastState.ulButtonPressed & vr::ButtonMaskFromId(button);
}

InputManager::OculusTouch::OculusTouch(vr::ETrackedControllerRole role)
	: m_Role(role)
	, m_StickTouched(false)
	, m_Gripped(false)
	, m_GrippedTime(0.0)
	, m_bHapticsRunning(true)
{
	memset(&m_LastState, 0, sizeof(m_LastState));
	m_ThumbStick.x = m_ThumbStick.y = 0.0f;

	m_HapticsThread = std::thread(HapticsThread, this);
}

InputManager::OculusTouch::~OculusTouch()
{
	m_bHapticsRunning = false;
	m_HapticsThread.join();
}

ovrControllerType InputManager::OculusTouch::GetType()
{
	return m_Role == vr::TrackedControllerRole_LeftHand ? ovrControllerType_LTouch : ovrControllerType_RTouch;
}

bool InputManager::OculusTouch::IsConnected()
{
	// Check if the Vive controller is assigned
	vr::TrackedDeviceIndex_t touch = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(m_Role);
	return touch != vr::k_unTrackedDeviceIndexInvalid;
}

bool InputManager::OculusTouch::GetInputState(ovrSession session, ovrInputState* inputState)
{
	// Get controller index
	vr::TrackedDeviceIndex_t touch = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(m_Role);
	ovrHandType hand = (m_Role == vr::TrackedControllerRole_LeftHand) ? ovrHand_Left : ovrHand_Right;

	if (touch == vr::k_unTrackedDeviceIndexInvalid)
		return false;

	vr::VRControllerState_t state;
	vr::VRSystem()->GetControllerState(touch, &state, sizeof(state));

	unsigned int buttons = 0, touches = 0;

	if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
		buttons |= (hand == ovrHand_Left) ? ovrButton_Enter : ovrButton_Home;

	if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
		buttons |= (hand == ovrHand_Left) ? ovrButton_LShoulder : ovrButton_RShoulder;

	// Allow users to enable a toggled grip.
	if (session->ToggleGrip == revGrip_Hybrid)
	{
		if (IsPressed(state, vr::k_EButton_Grip))
		{
			// Only set the timestamp on the first grip toggle, we don't want to toggle twice
			if (!m_Gripped)
				m_GrippedTime = ovr_GetTimeInSeconds();

			m_Gripped = true;
		}

		if (IsReleased(state, vr::k_EButton_Grip))
		{
			if (ovr_GetTimeInSeconds() - m_GrippedTime > session->ToggleDelay)
				m_Gripped = false;

			// Next time we always want to release grip
			m_GrippedTime = 0.0;
		}
	}
	else if (session->ToggleGrip == revGrip_Toggle)
	{
		if (IsPressed(state, vr::k_EButton_Grip))
			m_Gripped = !m_Gripped;
	}
	else
	{
		m_Gripped = !!(state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip));
	}

	if (state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
		touches |= (hand == ovrHand_Left) ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger;
	else if (m_Gripped)
		touches |= (hand == ovrHand_Left) ? ovrTouch_LIndexPointing : ovrTouch_RIndexPointing;


	// When we release the grip we need to keep it just a little bit pressed, because games like Toybox
	// can't handle a sudden jump to absolute zero.
	if (m_Gripped)
		inputState->HandTrigger[hand] = 1.0f;
	else
		inputState->HandTrigger[hand] = 0.1f;

	// Convert the axes
	for (int j = 0; j < vr::k_unControllerStateAxisCount; j++)
	{
		vr::ETrackedDeviceProperty prop = (vr::ETrackedDeviceProperty)(vr::Prop_Axis0Type_Int32 + j);
		vr::EVRControllerAxisType type = (vr::EVRControllerAxisType)vr::VRSystem()->GetInt32TrackedDeviceProperty(touch, prop);
		vr::VRControllerAxis_t axis = state.rAxis[j];
		vr::VRControllerAxis_t lastAxis = m_LastState.rAxis[j];

		if (type == vr::k_eControllerAxis_TrackPad)
		{
			ovrTouch quadrant = AxisToTouch(axis);

			if (state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
			{
				if (m_StickTouched)
				{
					ovrVector2f delta = { lastAxis.x - axis.x, lastAxis.y - axis.y };
					ovrVector2f stick = { m_ThumbStick.x - delta.x * session->Sensitivity, m_ThumbStick.y - delta.y * session->Sensitivity };

					//determine how far the controller is pushed
					float magnitude = sqrt(stick.x*stick.x + stick.y*stick.y);
					if (magnitude > 0.0f)
					{
						//determine the direction the controller is pushed
						float normalizedX = stick.x / magnitude;
						float normalizedY = stick.y / magnitude;

						//clip the magnitude at its expected maximum value and recenter
						if (magnitude > 1.0f) magnitude = 1.0f;
						m_ThumbStick.x = normalizedX * magnitude;
						m_ThumbStick.y = normalizedY * magnitude;

						inputState->ThumbstickNoDeadzone[hand].x = m_ThumbStick.x;
						inputState->ThumbstickNoDeadzone[hand].y = m_ThumbStick.y;
						if (magnitude > session->Deadzone)
						{
							//adjust magnitude relative to the end of the dead zone
							magnitude -= session->Deadzone;

							//optionally normalize the magnitude with respect to its expected range
							//giving a magnitude value of 0.0 to 1.0
							float normalizedMagnitude = magnitude / (1.0f - session->Deadzone);
							inputState->Thumbstick[hand].x = normalizedX * normalizedMagnitude;
							inputState->Thumbstick[hand].y = normalizedY * normalizedMagnitude;
						}
					}
				}

				if (quadrant & (ovrButton_LThumb | ovrButton_RThumb))
					m_StickTouched = true;

				touches |= quadrant;
			}
			else
			{
				// Touchpad was released, reset the thumbstick
				m_StickTouched = false;
				m_ThumbStick.x = m_ThumbStick.y = 0.0f;

				touches |= (hand == ovrHand_Left) ? ovrTouch_LThumbUp : ovrTouch_RThumbUp;
			}

			if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
				buttons |= quadrant;
		}

		if (type == vr::k_eControllerAxis_Trigger)
			inputState->IndexTrigger[hand] = axis.x;
	}

	// We don't apply deadzones yet on triggers and grips
	inputState->IndexTriggerNoDeadzone[hand] = inputState->IndexTrigger[hand];
	inputState->HandTriggerNoDeadzone[hand] = inputState->HandTrigger[hand];

	// Commit buttons/touches, count pressed buttons as touches
	inputState->Buttons |= buttons;
	inputState->Touches |= touches;

	// Save the state
	m_LastState = state;
	return true;
}

bool InputManager::OculusRemote::IsConnected()
{
	// Check if a Vive controller is available
	uint32_t controllerCount = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_Controller, nullptr, 0);

	// If only one controller is available, the Oculus Remote is connected
	return controllerCount == 1;
}

bool InputManager::OculusRemote::GetInputState(ovrSession session, ovrInputState* inputState)
{
	// Get controller indices.
	vr::TrackedDeviceIndex_t remote;
	vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_Controller, &remote, 1);

	if (remote == vr::k_unTrackedDeviceIndexInvalid)
		return false;

	vr::VRControllerState_t state;
	vr::VRSystem()->GetControllerState(remote, &state, sizeof(state));

	if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
		inputState->Buttons |= ovrButton_Back;

	// Convert the axes
	for (int i = 0; i < vr::k_unControllerStateAxisCount; i++)
	{
		vr::ETrackedDeviceProperty prop = (vr::ETrackedDeviceProperty)(vr::Prop_Axis0Type_Int32 + i);
		vr::EVRControllerAxisType type = (vr::EVRControllerAxisType)vr::VRSystem()->GetInt32TrackedDeviceProperty(remote, prop);
		vr::VRControllerAxis_t axis = state.rAxis[i];

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

	return state.ulButtonPressed != 0;
}

bool InputManager::XboxGamepad::IsConnected()
{
	// Check for Xbox controller
	XINPUT_STATE input;
	return XInputGetState(0, &input) == ERROR_SUCCESS;
}

bool InputManager::XboxGamepad::GetInputState(ovrSession session, ovrInputState* inputState)
{
	// Use XInput for Xbox controllers.
	XINPUT_STATE state;
	if (XInputGetState(0, &state) == ERROR_SUCCESS)
	{
		// Convert the buttons
		bool active = false;
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

		active = (buttons != 0);

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
				inputState->ThumbstickNoDeadzone[i].x = normalizedX;
				inputState->ThumbstickNoDeadzone[i].y = normalizedY;
				active = true;
			}

			if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
			{
				//clip the magnitude at its expected maximum value
				if (trigger > 255) trigger = 255;
				inputState->IndexTriggerNoDeadzone[i] = trigger / 255.0f;

				//adjust magnitude relative to the end of the dead zone
				trigger -= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

				//optionally normalize the magnitude with respect to its expected range
				//giving a magnitude value of 0.0 to 1.0
				float normalizedTrigger = trigger / (255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
				inputState->IndexTrigger[i] = normalizedTrigger;
				active = true;
			}
		}

		return active;
	}
	return false;
}

void InputManager::XboxGamepad::SetVibration(float frequency, float amplitude)
{
	// TODO: Disable the rumbler after a nominal amount of time
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
	XInputSetState(0, &vibration);
}
