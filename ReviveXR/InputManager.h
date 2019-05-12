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
	class InputDevice
	{
	public:
		InputDevice(XrSession session, const char* actionSetName, const char* localizedName);
		virtual ~InputDevice();

		// Input
		virtual ovrControllerType GetType() const = 0;
		virtual bool IsConnected() const = 0;
		virtual bool GetInputState(ovrControllerType controllerType, ovrInputState* inputState) = 0;

		// Bindings
		virtual XrPath GetSuggestedBindings(std::vector<XrActionSuggestedBinding>& outBindings) const { return XR_NULL_PATH; }
		virtual void GetActionSpaces(std::vector<XrSpace>& outSpaces) const { }
		virtual void GetActiveSets(std::vector<XrActiveActionSet>& outSets) const { }

		// Haptics
		virtual void SetVibration(ovrControllerType controllerType, float frequency, float amplitude) { }
		virtual void SubmitVibration(ovrControllerType controllerType, const ovrHapticsBuffer* buffer) { }
		virtual void GetVibrationState(ovrHandType hand, ovrHapticsPlaybackState* outState) { }

		operator XrActionSet() const { return m_ActionSet; }

	protected:
		static ovrVector2f ApplyDeadzone(ovrVector2f axis, float deadZone);
		static ovrButton TrackpadToDPad(ovrVector2f trackpad);

		XrActionSet m_ActionSet;
	};

	class Action
	{
	public:
		Action() : m_Action(XR_NULL_HANDLE) {}
		Action(InputDevice* device, XrActionType type, const char* actionName, const char* localizedName, bool handedAction = false);
		~Action();

		bool GetDigital(ovrHandType hand = ovrHand_Left) const;
		bool IsPressed(ovrHandType hand = ovrHand_Left) const;
		bool IsReleased(ovrHandType hand = ovrHand_Left) const;
		float GetAnalog(ovrHandType hand = ovrHand_Left) const;
		ovrVector2f GetVector(ovrHandType hand = ovrHand_Left) const;
		XrSpace GetSpace(ovrHandType hand = ovrHand_Left) const;

		operator XrAction() const { return m_Action; }

	private:
		bool m_IsHanded;
		XrAction m_Action;
		XrSpace m_Spaces[ovrHand_Count];
	};

	class OculusTouch : public InputDevice
	{
	public:
		OculusTouch(XrSession session);
		virtual ~OculusTouch();

		virtual ovrControllerType GetType() const override;
		virtual bool IsConnected() const override;
		virtual bool GetInputState(ovrControllerType controllerType, ovrInputState* inputState) override;
		virtual XrPath GetSuggestedBindings(std::vector<XrActionSuggestedBinding>& outBindings) const override;
		virtual void GetActionSpaces(std::vector<XrSpace>& outSpaces) const override;
		virtual void GetActiveSets(std::vector<XrActiveActionSet>& outSets) const override;

		virtual void SetVibration(ovrControllerType controllerType, float frequency, float amplitude) override;
		virtual void SubmitVibration(ovrControllerType controllerType, const ovrHapticsBuffer* buffer) override;
		virtual void GetVibrationState(ovrHandType hand, ovrHapticsPlaybackState* outState) override { *outState = m_HapticsBuffer[hand].GetState(); }

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

		Action m_Pose;
		Action m_Vibration;
		std::atomic_bool m_bHapticsRunning;
		HapticsBuffer m_HapticsBuffer[ovrHand_Count];

		std::thread m_HapticsThread;
		static void HapticsThread(OculusTouch* device);
	};

	class OculusRemote : public InputDevice
	{
	public:
		OculusRemote(XrSession session);
		virtual ~OculusRemote() { }

		virtual ovrControllerType GetType() const override { return ovrControllerType_Remote; }
		virtual bool IsConnected() const override;
		virtual bool GetInputState(ovrControllerType controllerType, ovrInputState* inputState) override;
		virtual void GetActiveSets(std::vector<XrActiveActionSet>& outSets) const override;

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
		XboxGamepad(XrSession session);
		virtual ~XboxGamepad();

		virtual ovrControllerType GetType() const override { return ovrControllerType_XBox; }
		virtual bool IsConnected() const override { return true; }
		virtual bool GetInputState(ovrControllerType controllerType, ovrInputState* inputState) override;
		virtual XrPath GetSuggestedBindings(std::vector<XrActionSuggestedBinding>& outBindings) const override;
		virtual void GetActiveSets(std::vector<XrActiveActionSet>& outSets) const override;
		virtual void SetVibration(ovrControllerType controllerType, float frequency, float amplitude) override;

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
		Action m_RVibration;
		Action m_LVibration;
	};

	InputManager(XrSession session);
	~InputManager();

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

