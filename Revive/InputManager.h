#pragma once

#include "OVR_CAPI.h"

#include <openvr.h>
#include <vector>
#include <Windows.h>
#include <Xinput.h>

typedef DWORD(__stdcall* _XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD(__stdcall* _XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

class InputManager
{
public:
	class InputDevice
	{
	public:
		// Input
		virtual ovrControllerType GetType() = 0;
		virtual bool IsConnected() = 0;
		virtual void GetInputState(ovrInputState* inputState) = 0;

		// Haptics
		// TODO: Implement support for Touch haptics.
		//virtual ovrTouchHapticsDesc GetTouchHapticsDesc() = 0;
		virtual void SetVibration(float frequency, float amplitude) = 0;
		//virtual void SubmitVibration(const ovrHapticsBuffer* buffer) = 0;
		//virtual void GetVibrationState(ovrHapticsPlaybackState* outState) = 0;
	};

	class OculusTouch : public InputDevice
	{
	public:
		OculusTouch(vr::ETrackedControllerRole role);
		~OculusTouch() { }

		virtual ovrControllerType GetType();
		virtual bool IsConnected();
		virtual void GetInputState(ovrInputState* inputState);
		virtual void SetVibration(float frequency, float amplitude);

	private:
		vr::ETrackedControllerRole m_Role;
		bool m_ThumbStick;
		bool m_MenuWasPressed;
		float m_ThumbStickRange;
	};

	class OculusRemote : public InputDevice
	{
	public:
		OculusRemote() { }
		~OculusRemote() { }

		virtual ovrControllerType GetType() { return ovrControllerType_Remote; };
		virtual bool IsConnected();
		virtual void GetInputState(ovrInputState* inputState);
		virtual void SetVibration(float frequency, float amplitude);
	};

	class XboxGamepad : public InputDevice
	{
	public:
		XboxGamepad() { }
		~XboxGamepad() { }

		virtual ovrControllerType GetType() { return ovrControllerType_XBox; };
		virtual bool IsConnected();
		virtual void GetInputState(ovrInputState* inputState);
		virtual void SetVibration(float frequency, float amplitude);
	};

	InputManager();
	~InputManager();

	unsigned int GetConnectedControllerTypes();
	ovrResult SetControllerVibration(ovrControllerType controllerType, float frequency, float amplitude);
	ovrResult GetInputState(ovrControllerType controllerType, ovrInputState* inputState);

protected:
	std::vector<InputDevice*> m_InputDevices;

private:
	HMODULE m_XInputLib;
	static _XInputGetState s_XInputGetState;
	static _XInputSetState s_XInputSetState;
};

