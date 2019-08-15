#pragma once

#include "HapticsBuffer.h"
#include "OVR_CAPI.h"
#include "Extras/OVR_Math.h"

#include <thread>
#include <vector>
#include <atomic>
#include <list>
#include <mutex>
#include <openvr.h>

class InputManager
{
public:
	class InputDevice
	{
	public:
		InputDevice(vr::VRActionSetHandle_t actionSet)
			: ActionSet(actionSet) { }
		virtual ~InputDevice() { }

		// Input
		virtual ovrControllerType GetType() = 0;
		virtual bool IsConnected() const = 0;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState) = 0;

		// Haptics
		virtual void SetVibration(float frequency, float amplitude) { }
		virtual void SubmitVibration(const ovrHapticsBuffer* buffer) { }
		virtual void GetVibrationState(ovrHapticsPlaybackState* outState) { }

		vr::VRActionSetHandle_t ActionSet;

	protected:
		static bool GetDigital(vr::VRActionHandle_t action)
		{
			vr::InputDigitalActionData_t data = {};
			vr::VRInput()->GetDigitalActionData(action, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
			return data.bState;
		}

		static bool IsPressed(vr::VRActionHandle_t action)
		{
			vr::InputDigitalActionData_t data = {};
			vr::VRInput()->GetDigitalActionData(action, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
			return data.bChanged && data.bState;
		}

		static bool IsReleased(vr::VRActionHandle_t action)
		{
			vr::InputDigitalActionData_t data = {};
			vr::VRInput()->GetDigitalActionData(action, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
			return data.bChanged && !data.bState;
		}

		static OVR::Vector2f GetAnalog(vr::VRActionHandle_t action)
		{
			vr::InputAnalogActionData_t data = {};
			vr::VRInput()->GetAnalogActionData(action, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
			return OVR::Vector2f(data.x, data.y);
		}

		static OVR::Vector2f ApplyDeadzone(OVR::Vector2f axis, float deadZoneLow, float deadZoneHigh);
	};

	class OculusTouch : public InputDevice
	{
	public:
		OculusTouch(vr::VRActionSetHandle_t actionSet, vr::ETrackedControllerRole role);
		virtual ~OculusTouch();

		virtual ovrControllerType GetType();
		virtual bool IsConnected() const;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState);

		virtual void SetVibration(float frequency, float amplitude) { m_Haptics.SetConstant(frequency, amplitude); }
		virtual void SubmitVibration(const ovrHapticsBuffer* buffer) { m_Haptics.AddSamples(buffer); }
		virtual void GetVibrationState(ovrHapticsPlaybackState* outState) { *outState = m_Haptics.GetState(); }

		vr::ETrackedControllerRole Role;

	private:
		vr::VRActionHandle_t m_Button_AX;
		vr::VRActionHandle_t m_Button_BY;
		vr::VRActionHandle_t m_Button_Thumb;
		vr::VRActionHandle_t m_Button_Enter;

		vr::VRActionHandle_t m_Touch_AX;
		vr::VRActionHandle_t m_Touch_BY;
		vr::VRActionHandle_t m_Touch_Thumb;
		vr::VRActionHandle_t m_Touch_ThumbRest;
		vr::VRActionHandle_t m_Touch_IndexTrigger;

		vr::VRActionHandle_t m_IndexTrigger;
		vr::VRActionHandle_t m_HandTrigger;
		vr::VRActionHandle_t m_Thumbstick;

		OVR::Vector2f m_Thumbstick_Center;
		vr::VRActionHandle_t m_Recenter_Thumb;

		vr::VRActionHandle_t m_Button_IndexTrigger;
		vr::VRActionHandle_t m_Button_HandTrigger;

		HapticsBuffer m_Haptics;
		std::atomic_bool m_bHapticsRunning;

		std::thread m_HapticsThread;
		static void HapticsThread(OculusTouch* device);
	};

	class OculusRemote : public InputDevice
	{
	public:
		OculusRemote(vr::VRActionSetHandle_t actionSet);
		virtual ~OculusRemote() { }

		virtual ovrControllerType GetType() { return ovrControllerType_Remote; }
		virtual bool IsConnected() const;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState);

	private:
		vr::VRActionHandle_t m_Button_Up;
		vr::VRActionHandle_t m_Button_Down;
		vr::VRActionHandle_t m_Button_Left;
		vr::VRActionHandle_t m_Button_Right;
		vr::VRActionHandle_t m_Button_Enter;
		vr::VRActionHandle_t m_Button_Back;
		vr::VRActionHandle_t m_Button_VolUp;
		vr::VRActionHandle_t m_Button_VolDown;
	};

	class XboxGamepad : public InputDevice
	{
	public:
		XboxGamepad(vr::VRActionSetHandle_t actionSet);
		virtual ~XboxGamepad();

		virtual ovrControllerType GetType() { return ovrControllerType_XBox; }
		virtual bool IsConnected() const { return true; }
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState);
		virtual void SetVibration(float frequency, float amplitude);

	private:
		vr::VRActionHandle_t m_Button_A;
		vr::VRActionHandle_t m_Button_B;
		vr::VRActionHandle_t m_Button_RThumb;
		vr::VRActionHandle_t m_Button_RShoulder;
		vr::VRActionHandle_t m_Button_X;
		vr::VRActionHandle_t m_Button_Y;
		vr::VRActionHandle_t m_Button_LThumb;
		vr::VRActionHandle_t m_Button_LShoulder;
		vr::VRActionHandle_t m_Button_Up;
		vr::VRActionHandle_t m_Button_Down;
		vr::VRActionHandle_t m_Button_Left;
		vr::VRActionHandle_t m_Button_Right;
		vr::VRActionHandle_t m_Button_Enter;
		vr::VRActionHandle_t m_Button_Back;
		vr::VRActionHandle_t m_RIndexTrigger;
		vr::VRActionHandle_t m_LIndexTrigger;
		vr::VRActionHandle_t m_RThumbstick;
		vr::VRActionHandle_t m_LThumbstick;
	};

	InputManager();
	~InputManager();

	std::atomic_uint32_t ConnectedControllers;

	void LoadActionManifest();
	void UpdateInputState();
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
	ovrPoseStatef m_LastPoses[vr::k_unMaxTrackedDeviceCount];

	unsigned int TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose);
	ovrPoseStatef TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, ovrPoseStatef& lastPose, double time);
};

