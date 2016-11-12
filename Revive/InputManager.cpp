#include "InputManager.h"
#include "OVR_CAPI.h"
#include "Common.h"

#include <openvr.h>
#include <Windows.h>
#include <Xinput.h>

bool InputManager::m_bHapticsRunning;

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

unsigned int InputManager::GetConnectedControllerTypes()
{
	unsigned int types = 0;

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
	inputState->ControllerType = controllerType;

	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
			device->GetInputState(inputState);
	}

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
		desc.SubmitMinSamples = 10;
		desc.SubmitOptimalSamples = 20;
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
		if (axis.x > 0.0f) {
			if (axis.y < 0.0f)
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
		if (axis.x < 0.0f) {
			if (axis.y < 0.0f)
				return ovrTouch_A;
			else
				return ovrTouch_B;
		}
		else {
			return ovrTouch_RThumb;
		}
	}
}

InputManager::OculusTouch::OculusTouch(vr::ETrackedControllerRole role)
	: m_Role(role)
	, m_ThumbRange(0.3f)
	, m_ThumbDeadzone(0.1f)
{
	memset(&m_LastState, 0, sizeof(m_LastState));
	memset(&m_ThumbCenter, 0, sizeof(m_ThumbCenter));

	// Get the thumb stick deadzone and range from the settings
	vr::EVRSettingsError error;
	float deadzone = vr::VRSettings()->GetFloat(REV_SETTINGS_SECTION, "ThumbDeadzone", &error);
	if (error == vr::VRSettingsError_None)
		m_ThumbDeadzone = deadzone;
	float range = vr::VRSettings()->GetFloat(REV_SETTINGS_SECTION, "ThumbRange", &error);
	if (range != 0.0f)
		m_ThumbRange = range;

	m_HapticsThread = std::thread(HapticsThread, this);
}

ovrControllerType InputManager::OculusTouch::GetType()
{
	return m_Role == vr::TrackedControllerRole_LeftHand ? ovrControllerType_LTouch : ovrControllerType_RTouch;
}

bool InputManager::OculusTouch::IsConnected()
{
	// Check if both Vive controllers are available
	uint32_t controllerCount = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_Controller, nullptr, 0);

	// If both controllers are available, the Oculus Touch is connected
	return controllerCount > 1;
}

void InputManager::OculusTouch::GetInputState(ovrInputState* inputState)
{
	// Get controller index
	vr::TrackedDeviceIndex_t touch = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(m_Role);
	ovrHandType hand = (m_Role == vr::TrackedControllerRole_LeftHand) ? ovrHand_Left : ovrHand_Right;

	if (touch == vr::k_unTrackedDeviceIndexInvalid)
		return;

	vr::VRControllerState_t state;
	vr::VRSystem()->GetControllerState(touch, &state);

	unsigned int buttons = 0, touches = 0;

	if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
		buttons |= (hand == ovrHand_Left) ? ovrButton_Enter : ovrButton_Home;

	if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
		buttons |= (hand == ovrHand_Left) ? ovrButton_LShoulder : ovrButton_RShoulder;

	if (state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
		touches |= (hand == ovrHand_Left) ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger;
	else if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip))
		touches |= (hand == ovrHand_Left) ? ovrTouch_LIndexPointing : ovrTouch_RIndexPointing;

	if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip))
		inputState->HandTrigger[hand] = 1.0f;

	// Convert the axes
	for (int j = 0; j < vr::k_unControllerStateAxisCount; j++)
	{
		vr::ETrackedDeviceProperty prop = (vr::ETrackedDeviceProperty)(vr::Prop_Axis0Type_Int32 + j);
		vr::EVRControllerAxisType type = (vr::EVRControllerAxisType)vr::VRSystem()->GetInt32TrackedDeviceProperty(touch, prop);
		vr::VRControllerAxis_t axis = state.rAxis[j];

		if (type == vr::k_eControllerAxis_TrackPad)
		{
			ovrTouch quadrant = AxisToTouch(axis);

			if (state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
			{
				// Always center the joystick at the point the pad was first touched
				if (!(m_LastState.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)))
					m_ThumbCenter = axis;

				if (quadrant & (ovrButton_LThumb | ovrButton_RThumb))
				{
					vr::VRControllerAxis_t stick = { (axis.x - m_ThumbCenter.x) / m_ThumbRange, (axis.y - m_ThumbCenter.y) / m_ThumbRange };

					//determine how far the controller is pushed
					float magnitude = sqrt(stick.x*stick.x + stick.y*stick.y);

					//determine the direction the controller is pushed
					float normalizedX = stick.x / magnitude;
					float normalizedY = stick.y / magnitude;

					//clip the magnitude at its expected maximum value
					if (magnitude > 1.0f) magnitude = 1.0f;
					inputState->ThumbstickNoDeadzone[hand].x = normalizedX * magnitude;
					inputState->ThumbstickNoDeadzone[hand].y = normalizedY * magnitude;
					
					if (magnitude > m_ThumbDeadzone)
					{
						//adjust magnitude relative to the end of the dead zone
						magnitude -= m_ThumbDeadzone;

						//optionally normalize the magnitude with respect to its expected range
						//giving a magnitude value of 0.0 to 1.0
						float normalizedMagnitude = magnitude / (1.0f - m_ThumbDeadzone);
						inputState->Thumbstick[hand].x = normalizedX * normalizedMagnitude;
						inputState->Thumbstick[hand].y = normalizedY * normalizedMagnitude;
					}
				}

				touches |= quadrant;
			}
			else
			{
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
}

bool InputManager::OculusRemote::IsConnected()
{
	// Check if a Vive controller is available
	uint32_t controllerCount = vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_Controller, nullptr, 0);

	// If one controller are available, the Oculus Remote is connected
	return controllerCount == 1;
}

void InputManager::OculusRemote::GetInputState(ovrInputState* inputState)
{
	// Get controller indices.
	vr::TrackedDeviceIndex_t remote;
	vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_Controller, &remote, 1);

	if (remote == vr::k_unTrackedDeviceIndexInvalid)
		return;

	vr::VRControllerState_t state;
	vr::VRSystem()->GetControllerState(remote, &state);

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
}

bool InputManager::XboxGamepad::IsConnected()
{
	// Check for Xbox controller
	XINPUT_STATE input;
	return XInputGetState(0, &input) == ERROR_SUCCESS;
}

void InputManager::XboxGamepad::GetInputState(ovrInputState* inputState)
{
	// Use XInput for Xbox controllers.
	XINPUT_STATE state;
	if (XInputGetState(0, &state) == ERROR_SUCCESS)
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
				inputState->ThumbstickNoDeadzone[i].x = normalizedX;
				inputState->ThumbstickNoDeadzone[i].y = normalizedY;
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
			}
		}
	}
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
