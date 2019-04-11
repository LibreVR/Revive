#include "InputManager.h"
#include "Common.h"
#include "Session.h"
#include "OVR_CAPI.h"
#include "XR_Math.h"

#include <Windows.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <vector>

#define WMR_COMPAT_PROFILES_ENABLED 1

XrPath InputManager::s_SubActionPaths[ovrHand_Count] = { XR_NULL_PATH, XR_NULL_PATH };

InputManager::InputManager(XrSession session)
	: m_InputDevices()
{
	s_SubActionPaths[ovrHand_Left] = GetXrPath("/user/hand/left");
	s_SubActionPaths[ovrHand_Right] = GetXrPath("/user/hand/right");

	m_InputDevices.push_back(new OculusTouch(session));
	m_InputDevices.push_back(new XboxGamepad(session));
	m_InputDevices.push_back(new OculusRemote(session));

	for (const InputDevice* device : m_InputDevices)
	{
		device->GetActiveSets(m_ActionSets);
		device->GetActionSpaces(m_ActionSpaces);

		std::vector<XrActionSuggestedBinding> bindings;
		XrPath profile = device->GetSuggestedBindings(bindings);
		if (!profile)
			continue;

		XrInteractionProfileSuggestedBinding suggestedBinding = XR_TYPE(INTERACTION_PROFILE_SUGGESTED_BINDING);
		suggestedBinding.countSuggestedBindings = bindings.size();
		suggestedBinding.suggestedBindings = bindings.data();
		suggestedBinding.interactionProfile = profile;
		XrResult rs = xrSetInteractionProfileSuggestedBindings(session, &suggestedBinding);
		assert(XR_SUCCEEDED(rs));
	}
}

InputManager::~InputManager()
{
	for (InputDevice* device : m_InputDevices)
		delete device;
}

ovrResult InputManager::SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	// Clamp the input
	frequency = std::min(std::max(frequency, 0.0f), 1.0f);
	amplitude = std::min(std::max(amplitude, 0.0f), 1.0f);

	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
			device->SetVibration(controllerType, frequency, amplitude);
	}

	return ovrSuccess;
}

ovrResult InputManager::GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	memset(inputState, 0, sizeof(ovrInputState));

	CHK_XR(xrSyncActionData(session->Session, (uint32_t)m_ActionSets.size(), m_ActionSets.data()));

	uint32_t types = 0;
	for (InputDevice* device : m_InputDevices)
	{
		ovrControllerType type = device->GetType();
		if (device->IsConnected())
		{
			if (controllerType & type)
			{
				if (device->GetInputState(controllerType, inputState))
					types |= type;
			}
		}
	}

	inputState->TimeInSeconds = ovr_GetTimeInSeconds();
	inputState->ControllerType = (ovrControllerType)types;
	return ovrSuccess;
}

ovrResult InputManager::SubmitControllerVibration(ovrSession session, ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
			device->SubmitVibration(controllerType, buffer);
	}

	return ovrSuccess;
}

ovrResult InputManager::GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState)
{
	memset(outState, 0, sizeof(ovrHapticsPlaybackState));

	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
			device->GetVibrationState(controllerType & ovrControllerType_RTouch ? ovrHand_Right : ovrHand_Left, outState);
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
		desc.SubmitMaxSamples = OVR_HAPTICS_BUFFER_SAMPLES_MAX;
		desc.SubmitMinSamples = 1;
		desc.SubmitOptimalSamples = 20;
		desc.QueueMinSizeToAvoidStarvation = 5;
	}

	return desc;
}

XrTime InputManager::AbsTimeToXrTime(XrInstance instance, double absTime)
{
	// Get back the XrTime
	static double PerfFrequency = 0.0;
	if (PerfFrequency == 0.0)
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		PerfFrequency = (double)freq.QuadPart;
	}

	XrResult rs;
	XrTime time;
	LARGE_INTEGER li;
	li.QuadPart = (LONGLONG)(absTime * PerfFrequency);
	rs = xrConvertWin32PerformanceCounterToTimeKHR(instance, &li, &time);
	assert(XR_SUCCEEDED(rs));
	return time;
}

unsigned int InputManager::SpaceRelationToPoseState(const XrSpaceRelation& relation, double time, ovrPoseStatef& outPoseState)
{
	if (relation.relationFlags & XR_SPACE_RELATION_ORIENTATION_VALID_BIT)
		outPoseState.ThePose.Orientation = XR::Quatf(relation.pose.orientation);
	else
		outPoseState.ThePose.Orientation = XR::Quatf::Identity();

	if (relation.relationFlags & XR_SPACE_RELATION_POSITION_VALID_BIT)
		outPoseState.ThePose.Position = XR::Vector3f(relation.pose.position);
	else
		outPoseState.ThePose.Position = XR::Vector3f::Zero();

	if (relation.relationFlags & XR_SPACE_RELATION_ANGULAR_VELOCITY_VALID_BIT)
		outPoseState.AngularVelocity = XR::Vector3f(relation.angularVelocity);
	else
		outPoseState.AngularVelocity = XR::Vector3f::Zero();

	if (relation.relationFlags & XR_SPACE_RELATION_LINEAR_VELOCITY_VALID_BIT)
		outPoseState.LinearVelocity = XR::Vector3f(relation.linearVelocity);
	else
		outPoseState.LinearVelocity = XR::Vector3f::Zero();

	if (relation.relationFlags & XR_SPACE_RELATION_ANGULAR_ACCELERATION_VALID_BIT)
		outPoseState.AngularAcceleration = XR::Vector3f(relation.angularAcceleration);
	else
		outPoseState.AngularAcceleration = XR::Vector3f::Zero();

	if (relation.relationFlags & XR_SPACE_RELATION_LINEAR_ACCELERATION_VALID_BIT)
		outPoseState.LinearAcceleration = XR::Vector3f(relation.linearAcceleration);
	else
		outPoseState.LinearAcceleration = XR::Vector3f::Zero();

	outPoseState.TimeInSeconds = time;

	return (relation.relationFlags >> 6) & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked);
}

void InputManager::GetTrackingState(ovrSession session, ovrTrackingState* outState, double absTime)
{
	if (absTime <= 0.0)
		absTime = ovr_GetTimeInSeconds();

	XrResult rs;
	XrSpaceRelation relation = XR_TYPE(SPACE_RELATION);
	XrTime displayTime = AbsTimeToXrTime(session->Instance, absTime);
	XrSpace space = (session->TrackingSpace == XR_REFERENCE_SPACE_TYPE_STAGE) ? session->StageSpace : session->LocalSpace;

	// Get space relation for the head
	rs = xrLocateSpace(session->ViewSpace, space, displayTime, &relation);
	assert(XR_SUCCEEDED(rs));
	outState->StatusFlags = SpaceRelationToPoseState(relation, absTime, outState->HeadPose);

	// Convert the hand poses
	for (int i = 0; i < ovrHand_Count; i++)
	{
		XrSpaceRelation handRelation = XR_TYPE(SPACE_RELATION);
		if (i < m_ActionSpaces.size())
		{
			rs = xrLocateSpace(m_ActionSpaces[i], space, displayTime, &handRelation);
			assert(XR_SUCCEEDED(rs));
		}

		outState->HandStatusFlags[i] = SpaceRelationToPoseState(handRelation, absTime, outState->HandPoses[i]);
	}

	outState->CalibratedOrigin = session->CalibratedOrigin;
}

ovrResult InputManager::GetDevicePoses(ovrSession session, ovrTrackedDeviceType* deviceTypes, int deviceCount, double absTime, ovrPoseStatef* outDevicePoses)
{
	XrTime displayTime = AbsTimeToXrTime(session->Instance, absTime);
	XrSpace space = (session->TrackingSpace == XR_REFERENCE_SPACE_TYPE_STAGE) ? session->StageSpace : session->LocalSpace;

	XrSpaceRelation relation = XR_TYPE(SPACE_RELATION);
	for (int i = 0; i < deviceCount; i++)
	{
		// Get the space for device types we recognize
		XrSpace space = XR_NULL_HANDLE;
		switch (deviceTypes[i])
		{
		case ovrTrackedDevice_HMD:
			space = session->ViewSpace;
			break;
		case ovrTrackedDevice_LTouch:
			space = m_ActionSpaces[ovrHand_Left];
			break;
		case ovrTrackedDevice_RTouch:
			space = m_ActionSpaces[ovrHand_Right];
			break;
		}

		if (space)
		{
			CHK_XR(xrLocateSpace(m_ActionSpaces[i], space, displayTime, &relation));
			SpaceRelationToPoseState(relation, absTime, outDevicePoses[i]);
		}
		else
		{
			outDevicePoses[i] = ovrPoseStatef{ OVR::Posef::Identity() };
		}
	}

	return ovrSuccess;
}

/* Action child-class */

InputManager::Action::Action(InputDevice* device, XrActionType type, const char* actionName, const char* localizedName, bool handedAction)
	: m_IsHanded(handedAction)
	, m_Action(XR_NULL_HANDLE)
	, m_Spaces()
{
	XrActionCreateInfo createInfo = XR_TYPE(ACTION_CREATE_INFO);
	createInfo.actionType = type;
	strcpy_s(createInfo.actionName, actionName);
	strcpy_s(createInfo.localizedActionName, localizedName);
	if (handedAction)
	{
		createInfo.countSubactionPaths = ovrHand_Count;
		createInfo.subactionPaths = s_SubActionPaths;
	}
	XrResult rs = xrCreateAction((XrActionSet)*device, &createInfo, &m_Action);
	assert(XR_SUCCEEDED(rs));

	if (type == XR_INPUT_ACTION_TYPE_POSE)
	{
		XrActionSpaceCreateInfo spaceInfo = XR_TYPE(ACTION_SPACE_CREATE_INFO);
		spaceInfo.poseInActionSpace = XR::Posef::Identity();
		spaceInfo.subactionPath = m_IsHanded ? s_SubActionPaths[ovrHand_Left] : XR_NULL_PATH;
		rs = xrCreateActionSpace(m_Action, &spaceInfo, &m_Spaces[ovrHand_Left]);
		assert(XR_SUCCEEDED(rs));

		if (m_IsHanded)
		{
			spaceInfo.subactionPath = s_SubActionPaths[ovrHand_Right];
			rs = xrCreateActionSpace(m_Action, &spaceInfo, &m_Spaces[ovrHand_Right]);
			assert(XR_SUCCEEDED(rs));
		}
	}
}

InputManager::Action::~Action()
{
	if (m_Action)
	{
		XrResult rs = xrDestroyAction(m_Action);
		assert(XR_SUCCEEDED(rs));
	}

	for (int i = 0; i < ovrHand_Count; i++)
	{
		if (m_Spaces[i])
		{
			XrResult rs = xrDestroySpace(m_Spaces[i]);
			assert(XR_SUCCEEDED(rs));
		}
	}
}

bool InputManager::Action::GetDigital(ovrHandType hand) const
{
	XrActionStateBoolean data = XR_TYPE(ACTION_STATE_BOOLEAN);
	XrResult rs = xrGetActionStateBoolean(m_Action, m_IsHanded ? 1 : 0, m_IsHanded ? &s_SubActionPaths[hand] : nullptr, &data);
	assert(XR_SUCCEEDED(rs));
	return data.currentState;
}

bool InputManager::Action::IsPressed(ovrHandType hand) const
{
	XrActionStateBoolean data = XR_TYPE(ACTION_STATE_BOOLEAN);
	XrResult rs = xrGetActionStateBoolean(m_Action, m_IsHanded ? 1 : 0, m_IsHanded ? &s_SubActionPaths[hand] : nullptr, &data);
	assert(XR_SUCCEEDED(rs));
	return data.changedSinceLastSync && data.currentState;
}

bool InputManager::Action::IsReleased(ovrHandType hand) const
{
	XrActionStateBoolean data = XR_TYPE(ACTION_STATE_BOOLEAN);
	XrResult rs = xrGetActionStateBoolean(m_Action, m_IsHanded ? 1 : 0, m_IsHanded ? &s_SubActionPaths[hand] : nullptr, &data);
	assert(XR_SUCCEEDED(rs));
	return data.changedSinceLastSync && !data.currentState;
}

float InputManager::Action::GetAnalog(ovrHandType hand) const
{
	XrActionStateVector1f data = XR_TYPE(ACTION_STATE_VECTOR1F);
	XrResult rs = xrGetActionStateVector1f(m_Action, m_IsHanded ? 1 : 0, m_IsHanded ? &s_SubActionPaths[hand] : nullptr, &data);
	assert(XR_SUCCEEDED(rs));
	return data.currentState;
}

ovrVector2f InputManager::Action::GetVector(ovrHandType hand) const
{
	XrActionStateVector2f data = XR_TYPE(ACTION_STATE_VECTOR2F);
	XrResult rs = xrGetActionStateVector2f(m_Action, m_IsHanded ? 1 : 0, m_IsHanded ? &s_SubActionPaths[hand] : nullptr, &data);
	assert(XR_SUCCEEDED(rs));
	return XR::Vector2f(data.currentState);
}

XrSpace InputManager::Action::GetSpace(ovrHandType hand) const
{
	return m_Spaces[hand];
}

/* Controller child-classes */

InputManager::InputDevice::InputDevice(XrSession session, const char* actionSetName, const char* localizedName)
{
	XrActionSetCreateInfo createInfo = XR_TYPE(ACTION_SET_CREATE_INFO);
	strcpy_s(createInfo.actionSetName, actionSetName);
	strcpy_s(createInfo.localizedActionSetName, localizedName);
	XrResult rs = xrCreateActionSet(session, &createInfo, &m_ActionSet);
	assert(XR_SUCCEEDED(rs));
}

InputManager::InputDevice::~InputDevice()
{
	XrResult rs = xrDestroyActionSet(m_ActionSet);
	assert(XR_SUCCEEDED(rs));
}

ovrVector2f InputManager::InputDevice::ApplyDeadzone(ovrVector2f axis, float deadZone)
{
	XR::Vector2f vector(axis);
	float mag = vector.Length();
	if (mag > deadZone)
	{
		// scale such that output magnitude is in the range[0, 1]
		float legalRange = 1.0f - deadZone;
		float normalizedMag = std::min(1.0f, (mag - deadZone) / legalRange);
		float scale = normalizedMag / mag;
		return vector * scale;
	}
	else
	{
		// stick is in the inner dead zone
		return OVR::Vector2f();
	}
}

ovrButton InputManager::InputDevice::TrackpadToDPad(ovrVector2f trackpad)
{
	if (trackpad.y < trackpad.x) {
		if (trackpad.y < -trackpad.x)
			return ovrButton_Down;
		else
			return ovrButton_Right;
	}
	else {
		if (trackpad.y < -trackpad.x)
			return ovrButton_Left;
		else
			return ovrButton_Up;
	}
}

void InputManager::OculusTouch::HapticsThread(OculusTouch* device)
{
	std::chrono::microseconds freq(std::chrono::seconds(1));
	freq /= REV_HAPTICS_SAMPLE_RATE;

	while (device->m_bHapticsRunning)
	{
		for (int i = 0; i < ovrHand_Count; i++)
		{
			float amplitude = device->m_HapticsBuffer[i].GetSample();
			if (amplitude > 0.0f)
			{
				XrHapticVibration vibration = XR_TYPE(HAPTIC_VIBRATION);
				vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
				vibration.amplitude = amplitude;
				vibration.duration = (XrDuration)std::chrono::nanoseconds(freq).count();
				XrResult rs = xrApplyHapticFeedback(device->m_Vibration, 1, &s_SubActionPaths[i], (XrHapticBaseHeader*)&vibration);
				assert(XR_SUCCEEDED(rs));
			}
		}

		std::this_thread::sleep_for(freq);
	}
}

InputManager::OculusTouch::OculusTouch(XrSession session)
	: InputDevice(session, "touch", "Oculus Touch")
	, m_bHapticsRunning(true)
	, m_Button_Enter(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "enter-click", "Menu button")
	, m_Button_AX(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "ax-click", "A/X pressed", true)
	, m_Button_BY(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "by-click", "B/Y pressed", true)
	, m_Button_Thumb(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "thumb-click", "Thumbstick pressed", true)
	, m_Touch_AX(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "ax-touch", "A/X touched", true)
	, m_Touch_BY(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "by-touch", "B/Y touched", true)
	, m_Touch_Thumb(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "thumb-touch", "Thumbstick touched", true)
	, m_Touch_ThumbRest(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "rest-touch", "Thumb rest touched", true)
	, m_Touch_IndexTrigger(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "trigger-touch", "Trigger touched", true)
	, m_IndexTrigger(this, XR_INPUT_ACTION_TYPE_VECTOR1F, "trigger", "Trigger button", true)
	, m_HandTrigger(this, XR_INPUT_ACTION_TYPE_VECTOR1F, "grip", "Grip button", true)
	, m_Thumbstick(this, XR_INPUT_ACTION_TYPE_VECTOR2F, "thumbstick", "Thumbstick", true)
	, m_Trackpad(this, XR_INPUT_ACTION_TYPE_VECTOR2F, "trackpad", "Trackpad", true)
	, m_TrackpadClick(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "trackpad-click", "Trackpad", true)
	, m_TrackpadTouch(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "trackpad-touch", "Trackpad", true)
	, m_Pose(this, XR_INPUT_ACTION_TYPE_POSE, "pose", "Controller pose", true)
	, m_Vibration(this, XR_OUTPUT_ACTION_TYPE_VIBRATION, "vibration", "Vibration", true)
{
	m_HapticsThread = std::thread(HapticsThread, this);
}

InputManager::OculusTouch::~OculusTouch()
{
	m_bHapticsRunning = false;
	m_HapticsThread.join();
}

XrPath InputManager::OculusTouch::GetSuggestedBindings(std::vector<XrActionSuggestedBinding>& outBindings) const
{
	std::string prefixes[ovrHand_Count] = { "/user/hand/left", "/user/hand/right" };

#define ADD_BINDING(action, path) outBindings.push_back(XrActionSuggestedBinding{ action, GetXrPath(path) })

	ADD_BINDING(m_Button_AX, "/user/hand/left/input/x/click");
	ADD_BINDING(m_Button_BY, "/user/hand/left/input/y/click");
	ADD_BINDING(m_Button_AX, "/user/hand/right/input/a/click");
	ADD_BINDING(m_Button_BY, "/user/hand/right/input/b/click");
	ADD_BINDING(m_Touch_AX, "/user/hand/left/input/x/touch");
	ADD_BINDING(m_Touch_BY, "/user/hand/left/input/y/touch");
	ADD_BINDING(m_Touch_AX, "/user/hand/right/input/a/touch");
	ADD_BINDING(m_Touch_BY, "/user/hand/right/input/b/touch");
	ADD_BINDING(m_Button_Enter, "/user/hand/left/input/menu/click");

	for (int i = 0; i < ovrHand_Count; i++)
	{
		ADD_BINDING(m_Button_Thumb, prefixes[i] + "/input/thumbstick/click");

		ADD_BINDING(m_Touch_Thumb, prefixes[i] + "/input/thumbstick");
		ADD_BINDING(m_Touch_ThumbRest, prefixes[i] + "/input/thumbrest/touch");
		ADD_BINDING(m_Touch_IndexTrigger, prefixes[i] + "/input/trigger/touch");

		ADD_BINDING(m_IndexTrigger, prefixes[i] + "/input/trigger/value");
#if WMR_COMPAT_PROFILES_ENABLED
		ADD_BINDING(m_HandTrigger, prefixes[i] + "/input/grip/click");
#else
		ADD_BINDING(m_HandTrigger, prefixes[i] + "/input/grip/value");
#endif
		ADD_BINDING(m_Thumbstick, prefixes[i] + "/input/thumbstick");

#if WMR_COMPAT_PROFILES_ENABLED
		ADD_BINDING(m_Trackpad, prefixes[i] + "/input/trackpad");
		ADD_BINDING(m_TrackpadClick, prefixes[i] + "/input/trackpad/click");
		ADD_BINDING(m_TrackpadTouch, prefixes[i] + "/input/trackpad/touch");
#endif

		ADD_BINDING(m_Pose, prefixes[i] + "/input/pointer/pose");
		ADD_BINDING(m_Vibration, prefixes[i] + "/output/haptic");
	}

#undef ADD_BINDING

#if WMR_COMPAT_PROFILES_ENABLED
	return GetXrPath("/interaction_profiles/microsoft/motion_controller");
#else
	return GetXrPath("/interaction_profiles/oculus/touch_controller");
#endif
}

void InputManager::OculusTouch::GetActionSpaces(std::vector<XrSpace>& outSpaces) const
{
	outSpaces.push_back(m_Pose.GetSpace(ovrHand_Left));
	outSpaces.push_back(m_Pose.GetSpace(ovrHand_Right));
}

void InputManager::OculusTouch::GetActiveSets(std::vector<XrActiveActionSet>& outSets) const
{
	XrActiveActionSet active = XR_TYPE(ACTIVE_ACTION_SET);
	active.actionSet = m_ActionSet;
	active.subactionPath = s_SubActionPaths[ovrHand_Left];
	outSets.push_back(active);
	active.subactionPath = s_SubActionPaths[ovrHand_Right];
	outSets.push_back(active);
}

ovrControllerType InputManager::OculusTouch::GetType() const
{
	return ovrControllerType_Touch;
}

bool InputManager::OculusTouch::IsConnected() const
{
	return true;
}

bool InputManager::OculusTouch::GetInputState(ovrControllerType controllerType, ovrInputState* inputState)
{
	if (m_Button_Enter.GetDigital())
		inputState->Buttons |= ovrButton_Enter;

	for (int i = 0; i < ovrHand_Count; i++)
	{
		ovrHandType hand = (ovrHandType)i;
		unsigned int buttons = 0, touches = 0;

		if (m_Button_AX.GetDigital(hand))
			buttons |= ovrButton_A;

		if (m_Touch_AX.GetDigital(hand))
			touches |= ovrTouch_A;

		if (m_Button_BY.GetDigital(hand))
			buttons |= ovrButton_B;

		if (m_Touch_BY.GetDigital(hand))
			touches |= ovrTouch_B;

		if (m_Button_Thumb.GetDigital(hand))
			buttons |= ovrButton_RThumb;

		if (m_Touch_Thumb.GetDigital(hand))
			touches |= ovrTouch_RThumb;

		if (m_Touch_ThumbRest.GetDigital(hand))
			touches |= ovrTouch_RThumbRest;

		if (m_Touch_IndexTrigger.GetDigital(hand))
			touches |= ovrTouch_RIndexTrigger;

#if WMR_COMPAT_PROFILES_ENABLED
		ovrButton dpad = TrackpadToDPad(m_Trackpad.GetVector(hand));
		if (dpad == ovrButton_Up ||
			(hand == ovrHand_Left && dpad == ovrButton_Right) ||
			(hand == ovrHand_Right && dpad == ovrButton_Left))
		{
			if (m_TrackpadClick.GetDigital(hand))
				buttons |= ovrButton_B;

			if (m_TrackpadTouch.GetDigital(hand))
				touches |= ovrTouch_B;
		}
		else
		{
			if (m_TrackpadClick.GetDigital(hand))
				buttons |= ovrButton_A;

			if (m_TrackpadTouch.GetDigital(hand))
				touches |= ovrTouch_A;
		}
#endif

		inputState->ThumbstickNoDeadzone[i] = m_Thumbstick.GetVector(hand);
		inputState->IndexTriggerNoDeadzone[i] = m_IndexTrigger.GetAnalog(hand);
		inputState->HandTriggerNoDeadzone[i] = m_HandTrigger.GetAnalog(hand);

#if WMR_COMPAT_PROFILES_ENABLED
		if (inputState->IndexTriggerNoDeadzone[i] > 0.1f)
		{
			touches |= ovrTouch_RIndexTrigger;
		}
#endif

		// Derive gestures from touch flags
		if (inputState->HandTriggerNoDeadzone[i] > 0.5f)
		{
			if (!(touches & ovrTouch_RIndexTrigger))
				touches |= ovrTouch_RIndexPointing;

			if (!(touches & ~(ovrTouch_RIndexTrigger | ovrTouch_RIndexPointing)))
				touches |= ovrTouch_RThumbUp;
		}

		// Apply deadzones where needed
		inputState->Thumbstick[i] = ApplyDeadzone(inputState->ThumbstickNoDeadzone[i], 0.24f);
		inputState->IndexTrigger[i] = inputState->IndexTriggerNoDeadzone[i];
		inputState->HandTrigger[i] = inputState->HandTriggerNoDeadzone[i];

		// We have no way to get raw values
		inputState->ThumbstickRaw[i] = inputState->ThumbstickNoDeadzone[i];
		inputState->IndexTriggerRaw[i] = inputState->IndexTriggerNoDeadzone[i];
		inputState->HandTriggerRaw[i] = inputState->HandTriggerNoDeadzone[i];

		inputState->Buttons |= (hand == ovrHand_Left) ? buttons << 8 : buttons;
		inputState->Touches |= (hand == ovrHand_Left) ? touches << 8 : touches;
	}

	return true;
}

void InputManager::OculusTouch::SetVibration(ovrControllerType controllerType, float frequency, float amplitude)
{
	std::vector<XrPath> subPaths;
	if (controllerType & ovrControllerType_LTouch)
		subPaths.push_back(s_SubActionPaths[ovrHand_Left]);
	if (controllerType & ovrControllerType_RTouch)
		subPaths.push_back(s_SubActionPaths[ovrHand_Right]);

	XrHapticVibration vibration = XR_TYPE(HAPTIC_VIBRATION);
	vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
	vibration.amplitude = amplitude;
	vibration.duration = 2500000000UL;
	XrResult rs = xrApplyHapticFeedback(m_Vibration, subPaths.size(), subPaths.empty() ? nullptr : subPaths.data(), (XrHapticBaseHeader*)&vibration);
	assert(XR_SUCCEEDED(rs));
}

void InputManager::OculusTouch::SubmitVibration(ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	if (controllerType & ovrControllerType_LTouch)
		m_HapticsBuffer[ovrHand_Left].AddSamples(buffer);
	if (controllerType & ovrControllerType_RTouch)
		m_HapticsBuffer[ovrHand_Right].AddSamples(buffer);
}

InputManager::OculusRemote::OculusRemote(XrSession session)
	: InputDevice(session, "remote", "Oculus Remote")
	, m_IsConnected(false)
	, m_Toggle_Connected(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "toggle-connect", "Connect the remote")
	, m_Button_Up(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "up-click", "Up pressed")
	, m_Button_Down(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "down-click", "Down pressed")
	, m_Button_Left(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "left-click", "Left pressed")
	, m_Button_Right(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "right-click", "Right pressed")
	, m_Button_Enter(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "enter-click", "Select pressed")
	, m_Button_Back(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "back-click", "Back pressed")
	, m_Button_VolUp(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "vol-up", "Volume up")
	, m_Button_VolDown(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "vol-down", "Volume down")
{
}

bool InputManager::OculusRemote::IsConnected() const
{
	return m_IsConnected;
}

bool InputManager::OculusRemote::GetInputState(ovrControllerType controllerType, ovrInputState* inputState)
{
	unsigned int buttons;

	// Allow the user to enable/disable the remote
	if (m_Toggle_Connected.IsPressed())
		m_IsConnected = !m_IsConnected;

	if (m_Button_Up.GetDigital())
		buttons |= ovrButton_Up;

	if (m_Button_Down.GetDigital())
		buttons |= ovrButton_Down;

	if (m_Button_Left.GetDigital())
		buttons |= ovrButton_Left;

	if (m_Button_Right.GetDigital())
		buttons |= ovrButton_Right;

	if (m_Button_Enter.GetDigital())
		buttons |= ovrButton_Enter;

	if (m_Button_Back.GetDigital())
		buttons |= ovrButton_Back;

	if (m_Button_VolUp.GetDigital())
		buttons |= ovrButton_VolUp;

	if (m_Button_VolDown.GetDigital())
		buttons |= ovrButton_VolDown;

	inputState->Buttons |= buttons;
	return true;
}

void InputManager::OculusRemote::GetActiveSets(std::vector<XrActiveActionSet>& outSets) const
{
	XrActiveActionSet active = XR_TYPE(ACTIVE_ACTION_SET);
	active.actionSet = m_ActionSet;
	outSets.push_back(active);
}

InputManager::XboxGamepad::XboxGamepad(XrSession session)
	: InputDevice(session, "xbox", "Xbox Controller")
	, m_Button_A(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "a-click", "A pressed")
	, m_Button_B(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "b-click", "B pressed")
	, m_Button_RThumb(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "rthumb-click", "Right thumbstick pressed")
	, m_Button_RShoulder(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "rshoulder-click", "Right shoulder pressed")
	, m_Button_X(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "x-click", "X pressed")
	, m_Button_Y(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "y-click", "Y pressed")
	, m_Button_LThumb(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "lthumb-click", "Left thumbstick pressed")
	, m_Button_LShoulder(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "lshoulder-click", "Left shoulder pressed")
	, m_Button_Up(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "up-click", "Up pressed")
	, m_Button_Down(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "down-click", "Down pressed")
	, m_Button_Left(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "left-click", "Left pressed")
	, m_Button_Right(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "right-click", "Right pressed")
	, m_Button_Enter(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "enter-click", "Select pressed")
	, m_Button_Back(this, XR_INPUT_ACTION_TYPE_BOOLEAN, "back-click", "Back pressed")
	, m_RIndexTrigger(this, XR_INPUT_ACTION_TYPE_VECTOR1F, "rtrigger", "Right trigger")
	, m_LIndexTrigger(this, XR_INPUT_ACTION_TYPE_VECTOR1F, "ltrigger", "Left trigger")
	, m_RThumbstick(this, XR_INPUT_ACTION_TYPE_VECTOR2F, "rthumb", "Right thumbstick")
	, m_LThumbstick(this, XR_INPUT_ACTION_TYPE_VECTOR2F, "lthumb", "Left thumbstick")
	, m_RVibration(this, XR_OUTPUT_ACTION_TYPE_VIBRATION, "rvibration", "Right vibration motor")
	, m_LVibration(this, XR_OUTPUT_ACTION_TYPE_VIBRATION, "lvibration", "Left vibration motor")
{
}

InputManager::XboxGamepad::~XboxGamepad()
{
}

XrPath InputManager::XboxGamepad::GetSuggestedBindings(std::vector<XrActionSuggestedBinding>& outBindings) const
{
#define ADD_BINDING(action, path) outBindings.push_back(XrActionSuggestedBinding{ action, GetXrPath(std::string("/user/gamepad") + path) })

	ADD_BINDING(m_Button_A, "/input/a/click");
	ADD_BINDING(m_Button_B, "/input/b/click");
	ADD_BINDING(m_Button_RThumb, "/input/thumbstick_right/click");
	ADD_BINDING(m_Button_RShoulder, "/input/shoulder_right/click");
	ADD_BINDING(m_Button_X, "/input/x/click");
	ADD_BINDING(m_Button_Y, "/input/y/click");
	ADD_BINDING(m_Button_LThumb, "/input/thumbstick_left/click");
	ADD_BINDING(m_Button_LShoulder, "/input/shoulder_left/click");
	ADD_BINDING(m_Button_Up, "/input/dpad_up/click");
	ADD_BINDING(m_Button_Down, "/input/dpad_down/click");
	ADD_BINDING(m_Button_Left, "/input/dpad_left/click");
	ADD_BINDING(m_Button_Right, "/input/dpad_right/click");
	ADD_BINDING(m_Button_Enter, "/input/menu/click");
	ADD_BINDING(m_Button_Back, "/input/view/click");
	ADD_BINDING(m_RIndexTrigger, "/input/trigger_right/value");
	ADD_BINDING(m_LIndexTrigger, "/input/trigger_left/value");
	ADD_BINDING(m_RThumbstick, "/input/thumbstick_right");
	ADD_BINDING(m_LThumbstick, "/input/thumbstick_left");
	ADD_BINDING(m_RVibration, "/output/haptic_right");
	ADD_BINDING(m_LVibration, "/output/haptic_left");

#undef ADD_BINDING

	return GetXrPath("/interaction_profiles/microsoft/xbox_controller");
}

bool InputManager::XboxGamepad::GetInputState(ovrControllerType controllerType, ovrInputState* inputState)
{
	unsigned int buttons = 0;

	if (m_Button_A.GetDigital())
		buttons |= ovrButton_A;

	if (m_Button_B.GetDigital())
		buttons |= ovrButton_B;

	if (m_Button_RThumb.GetDigital())
		buttons |= ovrButton_RThumb;

	if (m_Button_RShoulder.GetDigital())
		buttons |= ovrButton_RShoulder;

	if (m_Button_X.GetDigital())
		buttons |= ovrButton_X;

	if (m_Button_Y.GetDigital())
		buttons |= ovrButton_Y;

	if (m_Button_LThumb.GetDigital())
		buttons |= ovrButton_LThumb;

	if (m_Button_LShoulder.GetDigital())
		buttons |= ovrButton_LShoulder;

	if (m_Button_Up.GetDigital())
		buttons |= ovrButton_Up;

	if (m_Button_Down.GetDigital())
		buttons |= ovrButton_Down;

	if (m_Button_Left.GetDigital())
		buttons |= ovrButton_Left;

	if (m_Button_Right.GetDigital())
		buttons |= ovrButton_Right;

	if (m_Button_Enter.GetDigital())
		buttons |= ovrButton_Enter;

	if (m_Button_Back.GetDigital())
		buttons |= ovrButton_Back;

	const Action* triggers[] = { &m_LIndexTrigger, &m_RIndexTrigger };
	const Action* sticks[] = { &m_LThumbstick, &m_RThumbstick };
	float deadzone[] = { 0.24f, 0.265f };
	for (int hand = 0; hand < ovrHand_Count; hand++)
	{
		ovrVector2f thumbstick = sticks[hand]->GetVector();
		inputState->IndexTrigger[hand] = triggers[hand]->GetAnalog();
		inputState->Thumbstick[hand] = ApplyDeadzone(thumbstick, deadzone[hand]);
		inputState->ThumbstickNoDeadzone[hand] = thumbstick;

		// We don't apply deadzones yet on triggers and grips
		inputState->IndexTriggerNoDeadzone[hand] = inputState->IndexTrigger[hand];
		inputState->HandTriggerNoDeadzone[hand] = inputState->HandTrigger[hand];

		// We have no way to get raw values
		inputState->ThumbstickRaw[hand] = inputState->ThumbstickNoDeadzone[hand];
		inputState->IndexTriggerRaw[hand] = inputState->IndexTriggerNoDeadzone[hand];
		inputState->HandTriggerRaw[hand] = inputState->HandTriggerNoDeadzone[hand];
	}

	return true;
}

void InputManager::XboxGamepad::SetVibration(ovrControllerType controllerType, float frequency, float amplitude)
{
	XrHapticVibration vibration = XR_TYPE(HAPTIC_VIBRATION);
	vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
	vibration.amplitude = amplitude;
	vibration.duration = 2500000000UL;
	XrResult rs = xrApplyHapticFeedback(frequency > 0.5 ? m_RVibration : m_LVibration, 0, nullptr, (XrHapticBaseHeader*)&vibration);
	assert(XR_SUCCEEDED(rs));
}

void InputManager::XboxGamepad::GetActiveSets(std::vector<XrActiveActionSet>& outSets) const
{
	XrActiveActionSet active = XR_TYPE(ACTIVE_ACTION_SET);
	active.actionSet = m_ActionSet;
	outSets.push_back(active);
}
