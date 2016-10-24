#include "InputManager.h"
#include "OVR_CAPI.h"
#include "Common.h"

#include <openvr.h>
#include <Windows.h>
#include <Xinput.h>

_XInputGetState InputManager::s_XInputGetState;
_XInputSetState InputManager::s_XInputSetState;

InputManager::InputManager()
{
	m_XInputLib = LoadLibraryW(L"xinput1_3.dll");
	if (m_XInputLib)
	{
		s_XInputGetState = (_XInputGetState)GetProcAddress(m_XInputLib, "XInputGetState");
		s_XInputSetState = (_XInputSetState)GetProcAddress(m_XInputLib, "XInputSetState");
		m_InputDevices.push_back(new XboxGamepad());
	}

	m_InputDevices.push_back(new OculusTouch(vr::TrackedControllerRole_LeftHand));
	m_InputDevices.push_back(new OculusTouch(vr::TrackedControllerRole_RightHand));
	m_InputDevices.push_back(new OculusRemote());
}

InputManager::~InputManager()
{
	for (InputDevice* device : m_InputDevices)
		delete device;

	if (m_XInputLib)
		FreeLibrary(m_XInputLib);
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

InputManager::OculusTouch::OculusTouch(vr::ETrackedControllerRole role)
	: m_Role(role)
	, m_ThumbStick(role == vr::TrackedControllerRole_LeftHand)
	, m_MenuWasPressed(false)
	, m_ThumbStickRange(0.5f)
{
	float range = vr::VRSettings()->GetFloat(REV_SETTINGS_SECTION, "ThumbStickRange");
	if (range != 0.0f)
		m_ThumbStickRange = range;
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
	{
		if (!m_MenuWasPressed)
			m_ThumbStick = !m_ThumbStick;

		m_MenuWasPressed = true;
	}
	else
	{
		m_MenuWasPressed = false;
	}

	if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
		buttons |= (hand == ovrHand_Left) ? ovrButton_LShoulder : ovrButton_RShoulder;

	if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip))
		inputState->HandTrigger[hand] = 1.0f;

	if (state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
		touches |= (hand == ovrHand_Left) ? ovrTouch_LThumb : ovrTouch_RThumb;

	if (state.ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger))
		touches |= (hand == ovrHand_Left) ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger;

	// Convert the axes
	for (int j = 0; j < vr::k_unControllerStateAxisCount; j++)
	{
		vr::ETrackedDeviceProperty prop = (vr::ETrackedDeviceProperty)(vr::Prop_Axis0Type_Int32 + j);
		vr::EVRControllerAxisType type = (vr::EVRControllerAxisType)vr::VRSystem()->GetInt32TrackedDeviceProperty(touch, prop);
		vr::VRControllerAxis_t axis = state.rAxis[j];

		if (type == vr::k_eControllerAxis_TrackPad)
		{
			if (m_ThumbStick)
			{
				// Map the touchpad to the thumbstick with a slightly smaller range
				float mappedX = axis.x / m_ThumbStickRange;
				float mappedY = axis.y / m_ThumbStickRange;

				// Clip and assign the new values
				inputState->Thumbstick[hand].x = mappedX > 1.0f ? 1.0f : mappedX;
				inputState->Thumbstick[hand].y = mappedY > 1.0f ? 1.0f : mappedY;
			}

			if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad))
			{
				if (m_ThumbStick)
				{
					buttons |= (hand == ovrHand_Left) ? ovrButton_LThumb : ovrButton_RThumb;
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
			inputState->IndexTrigger[hand] = axis.x;
	}

	// We don't apply deadzones yet.
	inputState->IndexTriggerNoDeadzone[hand] = inputState->IndexTrigger[hand];
	inputState->HandTriggerNoDeadzone[hand] = inputState->HandTrigger[hand];
	inputState->ThumbstickNoDeadzone[hand] = inputState->Thumbstick[hand];

	// Commit buttons/touches, count pressed buttons as touches.
	inputState->Buttons |= buttons;
	inputState->Touches |= touches | buttons;
}

void InputManager::OculusTouch::SetVibration(float frequency, float amplitude)
{
	// TODO: Implement OpenVR haptics support
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

void InputManager::OculusRemote::SetVibration(float frequency, float amplitude)
{
	// TODO: Implement OpenVR haptics support
}

bool InputManager::XboxGamepad::IsConnected()
{
	// Check for Xbox controller
	XINPUT_STATE input;
	return s_XInputGetState(0, &input) == ERROR_SUCCESS;
}

void InputManager::XboxGamepad::GetInputState(ovrInputState* inputState)
{
	// Use XInput for Xbox controllers.
	XINPUT_STATE state;
	if (s_XInputGetState(0, &state) == ERROR_SUCCESS)
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
	s_XInputSetState(0, &vibration);
}
