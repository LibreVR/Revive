#include "InputManager.h"
#include "OVR_CAPI.h"
#include "Common.h"

#include <openvr.h>
#include <Windows.h>
#include <Xinput.h>

bool InputManager::m_bHapticsRunning;
float InputManager::m_Deadzone;
float InputManager::m_Sensitivity;
revGripType InputManager::m_ToggleGrip;
float InputManager::m_ToggleDelay;

HapticsBuffer::HapticsBuffer()
	: m_ReadIndex(0)
	, m_WriteIndex(0)
{
	memset(m_Buffer, 0, sizeof(m_Buffer));
}

void HapticsBuffer::AddSamples(const ovrHapticsBuffer* buffer)
{
	// Force constant vibration off
	m_ConstantTimeout = 0;

	uint8_t* samples = (uint8_t*)buffer->Samples;

	for (int i = 0; i < buffer->SamplesCount; i++)
	{
		if (m_WriteIndex == m_ReadIndex - 1)
			return;

		// Index will overflow correctly, so no need for a modulo operator
		m_Buffer[m_WriteIndex++] = samples[i];
	}
}

void HapticsBuffer::SetConstant(float frequency, float amplitude)
{
	// The documentation specifies a constant vibration should time out after 2.5 seconds
	m_Amplitude = amplitude;
	m_Frequency = frequency;
	m_ConstantTimeout = (uint16_t)(REV_HAPTICS_SAMPLE_RATE * 2.5);
}

float HapticsBuffer::GetSample()
{
	if (m_ConstantTimeout > 0)
	{
		float sample = m_Amplitude;
		if (m_Frequency <= 0.5f && m_ConstantTimeout % 2 == 0)
			sample = 0.0f;

		m_ConstantTimeout--;
		return sample;
	}

	// We can't pass the write index, so the buffer is now empty
	if (m_ReadIndex == m_WriteIndex)
		return 0.0f;

	// Index will overflow correctly, so no need for a modulo operator
	return m_Buffer[m_ReadIndex++] / 255.0f;
}

ovrHapticsPlaybackState HapticsBuffer::GetState()
{
	ovrHapticsPlaybackState state = { 0 };

	for (uint8_t i = m_WriteIndex; i != m_ReadIndex; i++)
		state.RemainingQueueSpace++;

	for (uint8_t i = m_ReadIndex; i != m_WriteIndex; i++)
		state.SamplesQueued++;

	return state;
}

InputManager::InputManager()
{
	LoadSettings();

	m_bHapticsRunning = true;

	m_InputDevices.push_back(new XboxGamepad());
	m_InputDevices.push_back(new OculusTouch(vr::TrackedControllerRole_LeftHand));
	m_InputDevices.push_back(new OculusTouch(vr::TrackedControllerRole_RightHand));
	m_InputDevices.push_back(new OculusRemote());
}

InputManager::~InputManager()
{
	m_bHapticsRunning = false;

	for (InputDevice* device : m_InputDevices)
		delete device;
}

void InputManager::LoadSettings()
{
	// Get the thumb stick deadzone and range from the settings
	m_Deadzone = ovr_GetFloat(nullptr, REV_KEY_THUMB_DEADZONE, REV_DEFAULT_THUMB_DEADZONE);
	m_Sensitivity = ovr_GetFloat(nullptr, REV_KEY_THUMB_SENSITIVITY, REV_DEFAULT_THUMB_SENSITIVITY);
	m_ToggleGrip = (revGripType)ovr_GetInt(nullptr, REV_KEY_TOGGLE_GRIP, REV_DEFAULT_TOGGLE_GRIP);
	m_ToggleDelay = ovr_GetFloat(nullptr, REV_KEY_TOGGLE_DELAY, REV_DEFAULT_TOGGLE_DELAY);
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

ovrResult InputManager::GetInputState(ovrControllerType controllerType, ovrInputState* inputState)
{
	memset(inputState, 0, sizeof(ovrInputState));

	inputState->TimeInSeconds = ovr_GetTimeInSeconds();

	uint32_t types = 0;
	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
		{
			if (device->GetInputState(inputState))
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

void InputManager::OculusTouch::HapticsThread(OculusTouch* device)
{
	std::chrono::microseconds freq(std::chrono::seconds(1));
	freq /= REV_HAPTICS_SAMPLE_RATE;

	while (m_bHapticsRunning)
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
{
	memset(&m_LastState, 0, sizeof(m_LastState));
	m_ThumbStick.x = m_ThumbStick.y = 0.0f;

	m_HapticsThread = std::thread(HapticsThread, this);
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

bool InputManager::OculusTouch::GetInputState(ovrInputState* inputState)
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
	if (m_ToggleGrip == revGrip_Hybrid)
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
			if (ovr_GetTimeInSeconds() - m_GrippedTime > m_ToggleDelay)
				m_Gripped = false;

			// Next time we always want to release grip
			m_GrippedTime = 0.0;
		}
	}
	else if (m_ToggleGrip == revGrip_Toggle)
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
					ovrVector2f stick = { m_ThumbStick.x - delta.x * m_Sensitivity, m_ThumbStick.y - delta.y * m_Sensitivity };

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
						if (magnitude > m_Deadzone)
						{
							//adjust magnitude relative to the end of the dead zone
							magnitude -= m_Deadzone;

							//optionally normalize the magnitude with respect to its expected range
							//giving a magnitude value of 0.0 to 1.0
							float normalizedMagnitude = magnitude / (1.0f - m_Deadzone);
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

bool InputManager::OculusRemote::GetInputState(ovrInputState* inputState)
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

bool InputManager::XboxGamepad::GetInputState(ovrInputState* inputState)
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
