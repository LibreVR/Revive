#pragma once

#include "Common.h"
#include "OVR_CAPI.h"
#include "HapticsBuffer.h"

#include <openxr/openxr.h>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

class InputManager
{
public:
	class Action
	{
	public:
		Action() : m_Action(XR_NULL_HANDLE) {}
		Action(XrActionSet set, XrActionType type, const char* actionName, const char* localizedName, bool handedAction = false);
		~Action();

		bool GetDigital(ovrHandType hand = ovrHand_Left) const;
		bool IsPressed(ovrHandType hand = ovrHand_Left) const;
		bool IsReleased(ovrHandType hand = ovrHand_Left) const;
		float GetAnalog(ovrHandType hand = ovrHand_Left) const;
		ovrVector2f GetVector(ovrHandType hand = ovrHand_Left) const;

		operator XrAction() { return m_Action; }

	private:
		bool m_IsHanded;
		XrAction m_Action;
	};

	class InputDevice
	{
	public:
		InputDevice(XrActionSet actionSet)
			: m_ActionSet(actionSet) { }
		virtual ~InputDevice() { }

		// Input
		virtual ovrControllerType GetType() const = 0;
		virtual bool IsConnected() const = 0;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState) = 0;

		// Haptics
		virtual void SetVibration(float frequency, float amplitude) { }
		virtual void SubmitVibration(const ovrHapticsBuffer* buffer) { }
		virtual void GetVibrationState(ovrHapticsPlaybackState* outState) { }

	protected:
		static ovrVector2f ApplyDeadzone(ovrVector2f axis, float deadZoneLow, float deadZoneHigh);
		static ovrButton TrackpadToDPad(ovrVector2f trackpad);

		XrActionSet m_ActionSet;
	};

	class OculusTouch : public InputDevice
	{
	public:
		OculusTouch(XrActionSet actionSet);
		virtual ~OculusTouch();

		virtual ovrControllerType GetType() const override;
		virtual bool IsConnected() const override;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState) override;

		virtual void SetVibration(float frequency, float amplitude) override;
		virtual void SubmitVibration(const ovrHapticsBuffer* buffer) override { m_HapticsBuffer.AddSamples(buffer); }
		virtual void GetVibrationState(ovrHapticsPlaybackState* outState) override { *outState = m_HapticsBuffer.GetState(); }

	private:
		Action m_Button_AX;
		Action m_Button_BY;
		Action m_Button_Thumb;
		Action m_Button_Enter;

		Action m_Touch_AX;
		Action m_Touch_BY;
		Action m_Touch_Thumb;
		Action m_Touch_ThumbRest;
		Action m_Touch_IndexTrigger;

		Action m_IndexTrigger;
		Action m_HandTrigger;
		Action m_Thumbstick;

		Action m_Trackpad;
		Action m_TrackpadClick;
		Action m_TrackpadTouch;

		Action m_Vibration;
		HapticsBuffer m_HapticsBuffer;
		std::atomic_bool m_bHapticsRunning;

		std::thread m_HapticsThread;
		static void HapticsThread(OculusTouch* device);
	};

	class OculusRemote : public InputDevice
	{
	public:
		OculusRemote(XrActionSet actionSet);
		virtual ~OculusRemote() { }

		virtual ovrControllerType GetType() const override { return ovrControllerType_Remote; }
		virtual bool IsConnected() const override;
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState) override;

	private:
		bool m_IsConnected;

		Action m_Toggle_Connected;
		Action m_Button_Up;
		Action m_Button_Down;
		Action m_Button_Left;
		Action m_Button_Right;
		Action m_Button_Enter;
		Action m_Button_Back;
		Action m_Button_VolUp;
		Action m_Button_VolDown;
	};

	class XboxGamepad : public InputDevice
	{
	public:
		XboxGamepad(XrActionSet actionSet);
		virtual ~XboxGamepad();

		virtual ovrControllerType GetType() const override { return ovrControllerType_XBox; }
		virtual bool IsConnected() const override { return true; }
		virtual bool GetInputState(ovrSession session, ovrInputState* inputState) override;
		virtual void SetVibration(float frequency, float amplitude) override;

	private:
		Action m_Button_A;
		Action m_Button_B;
		Action m_Button_RThumb;
		Action m_Button_RShoulder;
		Action m_Button_X;
		Action m_Button_Y;
		Action m_Button_LThumb;
		Action m_Button_LShoulder;
		Action m_Button_Up;
		Action m_Button_Down;
		Action m_Button_Left;
		Action m_Button_Right;
		Action m_Button_Enter;
		Action m_Button_Back;
		Action m_RIndexTrigger;
		Action m_LIndexTrigger;
		Action m_RThumbstick;
		Action m_LThumbstick;
	};

	InputManager(ovrSession session);
	~InputManager();

	unsigned int ConnectedControllers;

	ovrTouchHapticsDesc GetTouchHapticsDesc(ovrControllerType controllerType);
	ovrResult SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude);
	ovrResult GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState);
	ovrResult SubmitControllerVibration(ovrSession session, ovrControllerType controllerType, const ovrHapticsBuffer* buffer);
	ovrResult GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState);

	void GetTrackingState(ovrSession session, ovrTrackingState* outState, double absTime);
	ovrResult GetDevicePoses(ovrSession session, ovrTrackedDeviceType* deviceTypes, int deviceCount, double absTime, ovrPoseStatef* outDevicePoses);

protected:
	static XrPath s_SubActionPaths[ovrHand_Count];

	Action m_ControllerPose;
	std::vector<InputDevice*> m_InputDevices;
	std::vector<XrSpace> m_ActionSpaces;
	std::vector<XrActiveActionSet> m_ActionSets;

	static XrTime AbsTimeToXrTime(XrInstance instance, double absTime);
	static unsigned int SpaceRelationToPoseState(const XrSpaceRelation& relation, double time, ovrPoseStatef& outPoseState);
};

