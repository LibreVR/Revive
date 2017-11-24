#pragma once

#include "HapticsBuffer.h"
#include "OVR_CAPI.h"
#include "Extras/OVR_Math.h"

#include <thread>
#include <vector>
#include <atomic>
#include <openvr.h>
#include <Windows.h>
#include <Xinput.h>

typedef struct lua_State lua_State;

class InputManager
{
public:
	class InputDevice
	{
	public:
		virtual ~InputDevice() { }

		// Input
		virtual vr::ETrackedControllerRole GetRole() { return vr::TrackedControllerRole_Invalid; }
		virtual ovrControllerType GetType() = 0;
		virtual bool IsConnected() const = 0;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState) = 0;

		// Haptics
		virtual void SetVibration(float frequency, float amplitude) { }
		virtual void SubmitVibration(const ovrHapticsBuffer* buffer) { }
		virtual void GetVibrationState(ovrHapticsPlaybackState* outState) { }
	};

	class OculusTouch : public InputDevice
	{
	public:
		OculusTouch(vr::ETrackedControllerRole role);
		virtual ~OculusTouch();

		static const vr::EVRButtonId k_EButton_B = (vr::EVRButtonId)8;
		HapticsBuffer m_Haptics;

		virtual vr::ETrackedControllerRole GetRole() { return m_Role; }
		virtual ovrControllerType GetType();
		virtual bool IsConnected() const;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState);
		virtual void SetVibration(float frequency, float amplitude) { m_Haptics.SetConstant(frequency, amplitude); }
		virtual void SubmitVibration(const ovrHapticsBuffer* buffer) { m_Haptics.AddSamples(buffer); }
		virtual void GetVibrationState(ovrHapticsPlaybackState* outState) { *outState = m_Haptics.GetState(); }

	private:
		std::atomic_bool m_bHapticsRunning;
		vr::ETrackedControllerRole m_Role;
		vr::VRControllerState_t m_LastState;

		bool m_StickTouched;
		OVR::Vector2f m_ThumbStick;
		ovrTouch AxisToTouch(vr::VRControllerAxis_t axis);

		bool m_Gripped;
		double m_GrippedTime;
		bool IsPressed(vr::VRControllerState_t newState, vr::EVRButtonId button);
		bool IsReleased(vr::VRControllerState_t newState, vr::EVRButtonId button);

		std::thread m_HapticsThread;
		static void HapticsThread(OculusTouch* device);
	};

	class OculusRemote : public InputDevice
	{
	public:
		OculusRemote() { }
		virtual ~OculusRemote() { }

		virtual ovrControllerType GetType() { return ovrControllerType_Remote; }
		virtual bool IsConnected() const;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState);
	};

	class XboxGamepad : public InputDevice
	{
	public:
		XboxGamepad();
		virtual ~XboxGamepad();

		virtual ovrControllerType GetType() { return ovrControllerType_XBox; }
		virtual bool IsConnected() const;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState);
		virtual void SetVibration(float frequency, float amplitude);

	private:
		typedef DWORD(__stdcall* _XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
		typedef DWORD(__stdcall* _XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);

		HMODULE m_XInput;
		_XInputSetState SetState;
		_XInputGetState GetState;
	};

	InputManager();
	~InputManager();

	std::atomic_uint32_t ConnectedControllers;

	void UpdateConnectedControllers();
	ovrTouchHapticsDesc GetTouchHapticsDesc(ovrControllerType controllerType);
	ovrResult SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude);
	ovrResult GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState);
	ovrResult SubmitControllerVibration(ovrSession session, ovrControllerType controllerType, const ovrHapticsBuffer* buffer);
	ovrResult GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState);

	void GetTrackingState(ovrSession session, ovrTrackingState* outState, double absTime);
	ovrResult GetDevicePoses(ovrTrackedDeviceType* deviceTypes, int deviceCount, double absTime, ovrPoseStatef* outDevicePoses);

protected:
	std::vector<InputDevice*> m_InputDevices;

private:
	static lua_State *L;
	float m_fVsyncToPhotons;
	ovrPoseStatef m_LastPoses[vr::k_unMaxTrackedDeviceCount];

	unsigned int TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose);
	ovrPoseStatef TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, ovrPoseStatef& lastPose, double time);
};

