#include "InputManager.h"
#include "Common.h"
#include "Session.h"
#include "OVR_CAPI.h"
#include "XR_Math.h"

#include <Windows.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>

XrPath InputManager::s_SubActionPaths[ovrHand_Count] = { XR_NULL_PATH, XR_NULL_PATH };

InputManager::InputManager(ovrSession session)
	: ConnectedControllers(ovrControllerType_Touch | ovrControllerType_XBox)
	, m_InputDevices()
{
	XrResult rs;
	rs = xrStringToPath(session->Instance, "/user/hand/left", &s_SubActionPaths[ovrHand_Left]);
	assert(XR_SUCCEEDED(rs));
	rs = xrStringToPath(session->Instance, "/user/hand/right", &s_SubActionPaths[ovrHand_Right]);
	assert(XR_SUCCEEDED(rs));

	XrActionSet handle;
	XrActionSetCreateInfo createInfo = XR_TYPE(ACTION_SET_CREATE_INFO);
	XrActiveActionSet active = XR_TYPE(ACTIVE_ACTION_SET);
	active.subactionPath = XR_NULL_PATH;

	strcpy_s(createInfo.actionSetName, "touch");
	strcpy_s(createInfo.localizedActionSetName, "Oculus Touch");
	rs = xrCreateActionSet(session->Session, &createInfo, &handle);
	m_ControllerPose = Action(handle, XR_INPUT_ACTION_TYPE_POSE, "pose", "Controller pose", true);
	active.actionSet = handle;
	if (XR_SUCCEEDED(rs))
	{
		m_InputDevices.push_back(new OculusTouch(handle));
		active.subactionPath = s_SubActionPaths[ovrHand_Left];
		m_ActionSets.push_back(active);
		active.subactionPath = s_SubActionPaths[ovrHand_Right];
		m_ActionSets.push_back(active);
	}

	strcpy_s(createInfo.actionSetName, "xbox");
	strcpy_s(createInfo.localizedActionSetName, "Xbox Controller");
	rs = xrCreateActionSet(session->Session, &createInfo, &handle);
	active.actionSet = handle;
	if (XR_SUCCEEDED(rs))
	{
		m_InputDevices.push_back(new XboxGamepad(handle));
		active.subactionPath = XR_NULL_PATH;
		m_ActionSets.push_back(active);
	}

	strcpy_s(createInfo.actionSetName, "remote");
	strcpy_s(createInfo.localizedActionSetName, "Oculus Remote");
	rs = xrCreateActionSet(session->Session, &createInfo, &handle);
	active.actionSet = handle;
	if (XR_SUCCEEDED(rs))
	{
		m_InputDevices.push_back(new OculusRemote(handle));
		active.subactionPath = XR_NULL_PATH;
		m_ActionSets.push_back(active);
	}

	for (int i = 0; i < ovrHand_Count; i++)
	{
		XrSpace space;
		XrActionSpaceCreateInfo spaceInfo = XR_TYPE(ACTION_SPACE_CREATE_INFO);
		spaceInfo.poseInActionSpace = XR::Posef::Identity();
		spaceInfo.subactionPath = s_SubActionPaths[i];
		rs = xrCreateActionSpace(m_ControllerPose, &spaceInfo, &space);
		if (XR_SUCCEEDED(rs))
			m_ActionSpaces.push_back(space);
	}
}

InputManager::~InputManager()
{
	for (InputDevice* device : m_InputDevices)
		delete device;

	for (XrSpace space : m_ActionSpaces)
	{
		XrResult rs = xrDestroySpace(space);
		assert(XR_SUCCEEDED(rs));
	}

	for (XrActiveActionSet set : m_ActionSets)
	{
		XrResult rs = xrDestroyActionSet(set.actionSet);
		assert(XR_SUCCEEDED(rs));
	}
}

ovrResult InputManager::SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	// Clamp the input
	frequency = std::min(std::max(frequency, 0.0f), 1.0f);
	amplitude = std::min(std::max(amplitude, 0.0f), 1.0f);

	for (InputDevice* device : m_InputDevices)
	{
		if (controllerType & device->GetType() && device->IsConnected())
			device->SetVibration(frequency, amplitude);
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
			ConnectedControllers |= type;
			if (controllerType & type)
			{
				if (device->GetInputState(session, inputState))
					types |= type;
			}
		}
		else
		{
			ConnectedControllers &= ~type;
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
			device->SubmitVibration(buffer);
	}

	return ovrSuccess;
}

ovrResult InputManager::GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState)
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

		outState->HandStatusFlags[i] = SpaceRelationToPoseState(relation, absTime, outState->HandPoses[i]);
	}

	// TODO: Get the calibrated origin
	outState->CalibratedOrigin.Orientation = OVR::Quatf::Identity();
	outState->CalibratedOrigin.Position = OVR::Vector3f();
}

ovrResult InputManager::GetDevicePoses(ovrSession session, ovrTrackedDeviceType* deviceTypes, int deviceCount, double absTime, ovrPoseStatef* outDevicePoses)
{
	XrTime displayTime = AbsTimeToXrTime(session->Instance, absTime);
	XrSpace space = (session->TrackingSpace == XR_REFERENCE_SPACE_TYPE_STAGE) ? session->StageSpace : session->LocalSpace;

	XrSpaceRelation relation = XR_TYPE(SPACE_RELATION);
	for (int i = 0; i < deviceCount; i++)
	{
		if (i < m_ActionSpaces.size())
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

InputManager::Action::Action(XrActionSet set, XrActionType type, const char* actionName, const char* localizedName, bool handedAction)
	: m_IsHanded(handedAction)
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
	XrResult rs = xrCreateAction(set, &createInfo, &m_Action);
	assert(XR_SUCCEEDED(rs));
}

InputManager::Action::~Action()
{
	// TODO: Implement move contructor
#if 0
	if (m_Action)
	{
		XrResult rs = xrDestroyAction(m_Action);
		assert(XR_SUCCEEDED(rs));
	}
#endif
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

/* Controller child-classes */

ovrVector2f InputManager::InputDevice::ApplyDeadzone(ovrVector2f axis, float deadZoneLow, float deadZoneHigh)
{
	XR::Vector2f vector(axis);
	float mag = vector.Length();
	if (mag > deadZoneLow)
	{
		// scale such that output magnitude is in the range[0, 1]
		float legalRange = 1.0f - deadZoneHigh - deadZoneLow;
		float normalizedMag = std::min(1.0f, (mag - deadZoneLow) / legalRange);
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
		float amplitude = device->m_HapticsBuffer.GetSample();
		if (amplitude > 0.0f)
		{
			XrHapticVibration vibration = XR_TYPE(HAPTIC_VIBRATION);
			vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
			vibration.amplitude = amplitude;
			vibration.duration = (XrDuration)std::chrono::nanoseconds(freq).count();
			XrResult rs = xrApplyHapticFeedback(device->m_Vibration, ovrHand_Count, s_SubActionPaths, (XrHapticBaseHeader*)&vibration);
			assert(XR_SUCCEEDED(rs));
		}

		std::this_thread::sleep_for(freq);
	}
}

InputManager::OculusTouch::OculusTouch(XrActionSet actionSet)
	: InputDevice(actionSet)
	, m_bHapticsRunning(true)
	, m_Button_Enter(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "enter-click", "Menu button")
	, m_Button_AX(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "ax-click", "A/X pressed", true)
	, m_Button_BY(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "by-click", "B/Y pressed", true)
	, m_Button_Thumb(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "thumb-click", "Thumbstick pressed", true)
	, m_Touch_AX(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "ax-touch", "A/X touched", true)
	, m_Touch_BY(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "by-touch", "B/Y touched", true)
	, m_Touch_Thumb(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "thumb-touch", "Thumbstick touched", true)
	, m_Touch_ThumbRest(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "rest-touch", "Thumb rest touched", true)
	, m_Touch_IndexTrigger(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "trigger-touch", "Trigger touched", true)
	, m_IndexTrigger(actionSet, XR_INPUT_ACTION_TYPE_VECTOR1F, "trigger", "Trigger button", true)
	, m_HandTrigger(actionSet, XR_INPUT_ACTION_TYPE_VECTOR1F, "grip", "Grip button", true)
	, m_Thumbstick(actionSet, XR_INPUT_ACTION_TYPE_VECTOR2F, "thumbstick", "Thumbstick", true)
	, m_Trackpad(actionSet, XR_INPUT_ACTION_TYPE_VECTOR2F, "trackpad", "Trackpad", true)
	, m_TrackpadClick(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "trackpad-click", "Trackpad", true)
	, m_TrackpadTouch(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "trackpad-touch", "Trackpad", true)
	, m_Vibration(actionSet, XR_OUTPUT_ACTION_TYPE_VIBRATION, "vibration", "Vibration", true)
{
	m_HapticsThread = std::thread(HapticsThread, this);
}

InputManager::OculusTouch::~OculusTouch()
{
	m_bHapticsRunning = false;
	m_HapticsThread.join();
}

ovrControllerType InputManager::OculusTouch::GetType() const
{
	return ovrControllerType_Touch;
}

bool InputManager::OculusTouch::IsConnected() const
{
	return true;
}

bool InputManager::OculusTouch::GetInputState(ovrSession session, ovrInputState* inputState)
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

		if (m_Touch_IndexTrigger.GetDigital(hand))
			touches |= ovrTouch_RIndexTrigger;

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

		inputState->IndexTrigger[i] = m_IndexTrigger.GetAnalog(hand);
		inputState->HandTrigger[i] = m_HandTrigger.GetAnalog(hand);
		inputState->Thumbstick[i] = m_Thumbstick.GetVector(hand);

		// Derive gestures from touch flags
		// TODO: Should be handled with chords
		if (inputState->HandTrigger[i] > 0.5f)
		{
			if (!(touches & ovrTouch_RIndexTrigger))
				touches |= ovrTouch_RIndexPointing;

			if (!(touches & ~(ovrTouch_RIndexTrigger | ovrTouch_RIndexPointing)))
				touches |= ovrTouch_RThumbUp;
		}

		// We don't apply deadzones
		inputState->ThumbstickNoDeadzone[i] = inputState->Thumbstick[i];
		inputState->IndexTriggerNoDeadzone[i] = inputState->IndexTrigger[i];
		inputState->HandTriggerNoDeadzone[i] = inputState->HandTrigger[i];

		// We have no way to get raw values
		inputState->ThumbstickRaw[i] = inputState->ThumbstickNoDeadzone[i];
		inputState->IndexTriggerRaw[i] = inputState->IndexTriggerNoDeadzone[i];
		inputState->HandTriggerRaw[i] = inputState->HandTriggerNoDeadzone[i];

		inputState->Buttons |= buttons << (8 * i);
		inputState->Touches |= touches << (8 * i);
	}

	return true;
}

void InputManager::OculusTouch::SetVibration(float frequency, float amplitude)
{
	XrHapticVibration vibration = XR_TYPE(HAPTIC_VIBRATION);
	vibration.frequency = frequency;
	vibration.amplitude = amplitude;
	vibration.duration = 2500000000UL;
	XrResult rs = xrApplyHapticFeedback(m_Vibration, 0, nullptr, (XrHapticBaseHeader*)&vibration);
	assert(XR_SUCCEEDED(rs));
}

InputManager::OculusRemote::OculusRemote(XrActionSet actionSet)
	: InputDevice(actionSet)
	, m_IsConnected(false)
	, m_Toggle_Connected(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "toggle-connect", "Connect the remote")
	, m_Button_Up(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "up-click", "Up pressed")
	, m_Button_Down(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "down-click", "Down pressed")
	, m_Button_Left(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "left-click", "Left pressed")
	, m_Button_Right(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "right-click", "Right pressed")
	, m_Button_Enter(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "enter-click", "Select pressed")
	, m_Button_Back(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "back-click", "Back pressed")
	, m_Button_VolUp(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "vol-up", "Volume up")
	, m_Button_VolDown(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "vol-down", "Volume down")
{
}

bool InputManager::OculusRemote::IsConnected() const
{
	return m_IsConnected;
}

bool InputManager::OculusRemote::GetInputState(ovrSession session, ovrInputState* inputState)
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

InputManager::XboxGamepad::XboxGamepad(XrActionSet actionSet)
	: InputDevice(actionSet)
	, m_Button_A(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "a-click", "A pressed")
	, m_Button_B(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "b-click", "B pressed")
	, m_Button_RThumb(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "rthumb-click", "Right thumbstick pressed")
	, m_Button_RShoulder(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "rshoulder-click", "Right shoulder pressed")
	, m_Button_X(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "x-click", "X pressed")
	, m_Button_Y(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "y-click", "Y pressed")
	, m_Button_LThumb(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "lthumb-click", "Left thumbstick pressed")
	, m_Button_LShoulder(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "lshoulder-click", "Left shoulder pressed")
	, m_Button_Up(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "up-click", "Up pressed")
	, m_Button_Down(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "down-click", "Down pressed")
	, m_Button_Left(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "left-click", "Left pressed")
	, m_Button_Right(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "right-click", "Right pressed")
	, m_Button_Enter(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "enter-click", "Select pressed")
	, m_Button_Back(actionSet, XR_INPUT_ACTION_TYPE_BOOLEAN, "back-click", "Back pressed")
	, m_RIndexTrigger(actionSet, XR_INPUT_ACTION_TYPE_VECTOR1F, "rtrigger", "Right trigger")
	, m_LIndexTrigger(actionSet, XR_INPUT_ACTION_TYPE_VECTOR1F, "ltrigger", "Left trigger")
	, m_RThumbstick(actionSet, XR_INPUT_ACTION_TYPE_VECTOR2F, "rthumb", "Right thumbstick")
	, m_LThumbstick(actionSet, XR_INPUT_ACTION_TYPE_VECTOR2F, "lthumb", "Left thumbstick")
{
}

InputManager::XboxGamepad::~XboxGamepad()
{
}

bool InputManager::XboxGamepad::GetInputState(ovrSession session, ovrInputState* inputState)
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

	Action triggers[] = { m_LIndexTrigger, m_RIndexTrigger };
	Action sticks[] = { m_LThumbstick, m_RThumbstick };
	float deadzone[] = { 0.24f, 0.265f };
	for (int hand = 0; hand < ovrHand_Count; hand++)
	{
		ovrVector2f thumbstick = sticks[hand].GetVector();
		inputState->IndexTrigger[hand] = triggers[hand].GetAnalog();
		inputState->Thumbstick[hand] = ApplyDeadzone(thumbstick, deadzone[hand], deadzone[hand] / 2.0f);
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

void InputManager::XboxGamepad::SetVibration(float frequency, float amplitude)
{
}
