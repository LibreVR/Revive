#pragma once

#include "OVR_CAPI.h"

#include <atomic>
#include <openvr.h>
#include <thread>
#include <vector>
#include <Windows.h>
#include <Xinput.h>

#define REV_HAPTICS_SAMPLE_RATE 200
#define REV_HAPTICS_MAX_SAMPLES 256

class HapticsBuffer
{
public:
	HapticsBuffer();
	~HapticsBuffer() { }

	void AddSamples(const ovrHapticsBuffer* buffer);
	float GetSample();
	ovrHapticsPlaybackState GetState();

private:
	// Lock-less circular buffer
	std::atomic_uint8_t m_ReadIndex;
	std::atomic_uint8_t m_WriteIndex;
	uint8_t m_Buffer[REV_HAPTICS_MAX_SAMPLES];
};

class InputManager
{
public:
	class InputDevice
	{
	public:
		// Input
		virtual vr::ETrackedControllerRole GetRole() { return vr::TrackedControllerRole_Invalid; }
		virtual ovrControllerType GetType() = 0;
		virtual bool IsConnected() = 0;
		virtual void GetInputState(ovrInputState* inputState) = 0;

		// Haptics
		virtual void SetVibration(float frequency, float amplitude) { }
		virtual void SubmitVibration(const ovrHapticsBuffer* buffer) { }
		virtual void GetVibrationState(ovrHapticsPlaybackState* outState) { }
	};

	class OculusTouch : public InputDevice
	{
	public:
		OculusTouch(vr::ETrackedControllerRole role);
		~OculusTouch() { m_HapticsThread.join(); }

		HapticsBuffer m_Haptics;

		virtual vr::ETrackedControllerRole GetRole() { return m_Role; }
		virtual ovrControllerType GetType();
		virtual bool IsConnected();
		virtual void GetInputState(ovrInputState* inputState);
		virtual void SetVibration(float frequency, float amplitude);
		virtual void SubmitVibration(const ovrHapticsBuffer* buffer) { m_Haptics.AddSamples(buffer); }
		virtual void GetVibrationState(ovrHapticsPlaybackState* outState) { *outState = m_Haptics.GetState(); }

	private:
		vr::ETrackedControllerRole m_Role;
		bool m_ThumbStick;
		bool m_MenuWasPressed;
		float m_ThumbStickRange;

		std::thread m_HapticsThread;
		static void HapticsThread(OculusTouch* device);
	};

	class OculusRemote : public InputDevice
	{
	public:
		OculusRemote() { }
		~OculusRemote() { }

		virtual ovrControllerType GetType() { return ovrControllerType_Remote; }
		virtual bool IsConnected();
		virtual void GetInputState(ovrInputState* inputState);
	};

	class XboxGamepad : public InputDevice
	{
	public:
		XboxGamepad() { }
		~XboxGamepad() { }

		virtual ovrControllerType GetType() { return ovrControllerType_XBox; }
		virtual bool IsConnected();
		virtual void GetInputState(ovrInputState* inputState);
		virtual void SetVibration(float frequency, float amplitude);
	};

	InputManager();
	~InputManager();

	unsigned int GetConnectedControllerTypes();
	ovrTouchHapticsDesc GetTouchHapticsDesc(ovrControllerType controllerType);

	ovrResult SetControllerVibration(ovrControllerType controllerType, float frequency, float amplitude);
	ovrResult GetInputState(ovrControllerType controllerType, ovrInputState* inputState);
	ovrResult SubmitControllerVibration(ovrControllerType controllerType, const ovrHapticsBuffer* buffer);
	ovrResult GetControllerVibrationState(ovrControllerType controllerType, ovrHapticsPlaybackState* outState);

protected:
	std::vector<InputDevice*> m_InputDevices;
	static bool m_bHapticsRunning;
};

