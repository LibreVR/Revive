#include "InputManager.h"
#include "Common.h"
#include "Session.h"
#include "Runtime.h"
#include "OVR_CAPI.h"
#include "XR_Math.h"

#include <Windows.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <vector>

XrPath InputManager::s_SubActionPaths[ovrHand_Count] = { XR_NULL_PATH, XR_NULL_PATH };

InputManager::InputManager(XrInstance instance)
	: m_InputDevices()
{
	s_SubActionPaths[ovrHand_Left] = GetXrPath("/user/hand/left");
	s_SubActionPaths[ovrHand_Right] = GetXrPath("/user/hand/right");

	m_InputDevices.push_back(new OculusTouch(instance));
	m_InputDevices.push_back(new XboxGamepad(instance));
	m_InputDevices.push_back(new OculusRemote(instance));

	for (const InputDevice* device : m_InputDevices)
	{
		device->GetActiveSets(m_ActionSets);

		std::vector<XrActionSuggestedBinding> bindings;
		XrPath profile = device->GetSuggestedBindings(bindings);
		if (!profile)
			continue;

		XrInteractionProfileSuggestedBinding suggestedBinding = XR_TYPE(INTERACTION_PROFILE_SUGGESTED_BINDING);
		suggestedBinding.countSuggestedBindings = (uint32_t)bindings.size();
		suggestedBinding.suggestedBindings = bindings.data();
		suggestedBinding.interactionProfile = profile;
		XrResult rs = xrSuggestInteractionProfileBindings(instance, &suggestedBinding);
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
			CHK_OVR(device->SetVibration(session->Session, controllerType, frequency, amplitude));
	}

	return ovrSuccess;
}

ovrResult InputManager::GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	memset(inputState, 0, sizeof(ovrInputState));

	XrActionsSyncInfo syncInfo = XR_TYPE(ACTIONS_SYNC_INFO);
	syncInfo.countActiveActionSets = (uint32_t)m_ActionSets.size();
	syncInfo.activeActionSets = m_ActionSets.data();
	CHK_XR(xrSyncActions(session->Session, &syncInfo));

	uint32_t types = 0;
	for (InputDevice* device : m_InputDevices)
	{
		ovrControllerType type = device->GetType();
		if (device->IsConnected())
		{
			if (controllerType & type)
			{
				if (device->GetInputState(session->Session, controllerType, inputState))
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
		if (controllerType & device->GetType() && device->IsConnected()) {
			device->StartHaptics(session->Session);
			device->SubmitVibration(controllerType, buffer);
		}
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

unsigned int InputManager::SpaceRelationToPoseState(const XrSpaceLocation& location, double time, ovrPoseStatef& lastPoseState, ovrPoseStatef& outPoseState)
{
	unsigned int flags = 0;

	if (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
   {
		outPoseState.ThePose.Orientation = XR::Quatf(location.pose.orientation);
		flags |= ovrStatus_OrientationValid;
		flags |= (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) ? ovrStatus_OrientationTracked : 0;
	}
	else
   {
		outPoseState.ThePose.Orientation = XR::Quatf::Identity();
	}

	if (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)
   {
		outPoseState.ThePose.Position = XR::Vector3f(location.pose.position);
		flags |= ovrStatus_PositionValid;
		flags |= (location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) ? ovrStatus_PositionTracked : 0;
	}
	else
   {
		outPoseState.ThePose.Position = XR::Vector3f::Zero();
	}

	XrSpaceVelocity *spaceVelocity = (XrSpaceVelocity *) location.next;

	if (spaceVelocity->velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT)
   {
		XR::Vector3f currav(spaceVelocity->angularVelocity);
		outPoseState.AngularVelocity = currav;
		outPoseState.AngularAcceleration = time > lastPoseState.TimeInSeconds ? (currav - XR::Vector3f(lastPoseState.AngularVelocity)) / float(time - lastPoseState.TimeInSeconds) : lastPoseState.AngularAcceleration;
	}
	else
   {
		outPoseState.AngularVelocity = XR::Vector3f::Zero();
		outPoseState.AngularAcceleration = XR::Vector3f::Zero();
	}

	if (spaceVelocity->velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT)
   {
		XR::Vector3f currlv(spaceVelocity->linearVelocity);
		outPoseState.LinearVelocity = currlv;
		outPoseState.LinearAcceleration = time > lastPoseState.TimeInSeconds ? (currlv - XR::Vector3f(lastPoseState.LinearVelocity)) / float(time - lastPoseState.TimeInSeconds) : lastPoseState.LinearAcceleration;
	}
	else
   {
		outPoseState.LinearVelocity = XR::Vector3f::Zero();
		outPoseState.LinearAcceleration = XR::Vector3f::Zero();
	}

	outPoseState.TimeInSeconds = time;

	return flags;
}

void InputManager::GetTrackingState(ovrSession session, ovrTrackingState* outState, double absTime)
{
	double calledTime = absTime;

	if (!session->Session)
		return;

	XrSpaceLocation location = XR_TYPE(SPACE_LOCATION);
	XrSpaceVelocity velocity = XR_TYPE(SPACE_VELOCITY);
	location.next = &velocity;
	XrTime displayTime = absTime <= 0.0 ? AbsTimeToXrTime(session->Instance, ovr_GetTimeInSeconds()) : AbsTimeToXrTime(session->Instance, absTime);
	XrSpace space = (session->TrackingSpace == XR_REFERENCE_SPACE_TYPE_STAGE) ? session->StageSpace : session->LocalSpace;

	// Get space relation for the head
	if (XR_SUCCEEDED(xrLocateSpace(session->ViewSpace, space, displayTime, &location)))
		outState->StatusFlags = SpaceRelationToPoseState(location, absTime, m_LastTrackingState.HeadPose, outState->HeadPose);

	// Convert the hand poses
	for (uint32_t i = 0; i < ovrHand_Count; i++)
	{
		XrSpaceLocation handLocation = XR_TYPE(SPACE_LOCATION);
		handLocation.next = &velocity;
		if (i < m_ActionSpaces.size() && XR_SUCCEEDED(xrLocateSpace(m_ActionSpaces[i], space, displayTime, &handLocation)))
			outState->HandStatusFlags[i] = SpaceRelationToPoseState(handLocation, absTime, m_LastTrackingState.HandPoses[i], outState->HandPoses[i]);
	}

	m_LastTrackingState = *outState;

	outState->CalibratedOrigin = session->CalibratedOrigin;
}

ovrResult InputManager::GetDevicePoses(ovrSession session, ovrTrackedDeviceType* deviceTypes, int deviceCount, double absTime, ovrPoseStatef* outDevicePoses)
{
	XrTime displayTime = absTime <= 0.0 ? (*session->CurrentFrame).predictedDisplayTime : AbsTimeToXrTime(session->Instance, absTime);
	XrSpace space = (session->TrackingSpace == XR_REFERENCE_SPACE_TYPE_STAGE) ? session->StageSpace : session->LocalSpace;

	XrSpaceLocation location = XR_TYPE(SPACE_LOCATION);
	XrSpaceVelocity velocity = XR_TYPE(SPACE_VELOCITY);
	location.next = &velocity;
	for (int i = 0; i < deviceCount; i++)
	{
		// Get the space for device types we recognize
		XrSpace space = XR_NULL_HANDLE;
      ovrPoseStatef* pLastState = nullptr;
		switch (deviceTypes[i])
		{
		case ovrTrackedDevice_HMD:
			space = session->ViewSpace;
         pLastState = &m_LastTrackingState.HeadPose;
			break;
		case ovrTrackedDevice_LTouch:
			space = m_ActionSpaces[ovrHand_Left];
         pLastState = &m_LastTrackingState.HandPoses[ovrHand_Left];
			break;
		case ovrTrackedDevice_RTouch:
			space = m_ActionSpaces[ovrHand_Right];
         pLastState = &m_LastTrackingState.HandPoses[ovrHand_Right];
			break;
		}

		if (space && pLastState)
		{
			CHK_XR(xrLocateSpace(m_ActionSpaces[i], space, displayTime, &location));
			SpaceRelationToPoseState(location, absTime, *pLastState, outDevicePoses[i]);
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
	XrResult rs = xrCreateAction(device->ActionSet(), &createInfo, &m_Action);
	assert(XR_SUCCEEDED(rs));
}

bool InputManager::Action::GetDigital(XrSession session, ovrHandType hand) const
{
	XrActionStateGetInfo info = XR_TYPE(ACTION_STATE_GET_INFO);
	info.action = m_Action;
	info.subactionPath = m_IsHanded ? s_SubActionPaths[hand] : XR_NULL_PATH;
	XrActionStateBoolean data = XR_TYPE(ACTION_STATE_BOOLEAN);
	XrResult rs = xrGetActionStateBoolean(session, &info, &data);
	assert(XR_SUCCEEDED(rs));
	return data.currentState;
}

bool InputManager::Action::IsPressed(XrSession session, ovrHandType hand) const
{
	XrActionStateGetInfo info = XR_TYPE(ACTION_STATE_GET_INFO);
	info.action = m_Action;
	info.subactionPath = m_IsHanded ? s_SubActionPaths[hand] : XR_NULL_PATH;
	XrActionStateBoolean data = XR_TYPE(ACTION_STATE_BOOLEAN);
	XrResult rs = xrGetActionStateBoolean(session, &info, &data);
	assert(XR_SUCCEEDED(rs));
	return data.changedSinceLastSync && data.currentState;
}

bool InputManager::Action::IsReleased(XrSession session, ovrHandType hand) const
{
	XrActionStateGetInfo info = XR_TYPE(ACTION_STATE_GET_INFO);
	info.action = m_Action;
	info.subactionPath = m_IsHanded ? s_SubActionPaths[hand] : XR_NULL_PATH;
	XrActionStateBoolean data = XR_TYPE(ACTION_STATE_BOOLEAN);
	XrResult rs = xrGetActionStateBoolean(session, &info, &data);
	assert(XR_SUCCEEDED(rs));
	return data.changedSinceLastSync && !data.currentState;
}

float InputManager::Action::GetAnalog(XrSession session, ovrHandType hand) const
{
	XrActionStateGetInfo info = XR_TYPE(ACTION_STATE_GET_INFO);
	info.action = m_Action;
	info.subactionPath = m_IsHanded ? s_SubActionPaths[hand] : XR_NULL_PATH;
	XrActionStateFloat data = XR_TYPE(ACTION_STATE_FLOAT);
	XrResult rs = xrGetActionStateFloat(session, &info, &data);
	assert(XR_SUCCEEDED(rs));
	return data.currentState;
}

ovrVector2f InputManager::Action::GetVector(XrSession session, ovrHandType hand) const
{
	XrActionStateGetInfo info = XR_TYPE(ACTION_STATE_GET_INFO);
	info.action = m_Action;
	info.subactionPath = m_IsHanded ? s_SubActionPaths[hand] : XR_NULL_PATH;
	XrActionStateVector2f data = XR_TYPE(ACTION_STATE_VECTOR2F);
	XrResult rs = xrGetActionStateVector2f(session, &info, &data);
	assert(XR_SUCCEEDED(rs));
	return XR::Vector2f(data.currentState);
}

XrSpace InputManager::Action::CreateSpace(XrSession session, ovrHandType hand) const
{
	XrSpace space;
	XrActionSpaceCreateInfo spaceInfo = XR_TYPE(ACTION_SPACE_CREATE_INFO);
	spaceInfo.action = m_Action;
	spaceInfo.subactionPath = m_IsHanded ? s_SubActionPaths[hand] : XR_NULL_PATH;
	spaceInfo.poseInActionSpace = XR::Posef::Identity();
	XrResult rs = xrCreateActionSpace(session, &spaceInfo, &space);
	assert(XR_SUCCEEDED(rs));
	return space;
}

/* Controller child-classes */

InputManager::InputDevice::InputDevice(XrInstance instance, const char* actionSetName, const char* localizedName)
{
	XrActionSetCreateInfo createInfo = XR_TYPE(ACTION_SET_CREATE_INFO);
	strcpy_s(createInfo.actionSetName, actionSetName);
	strcpy_s(createInfo.localizedActionSetName, localizedName);
	XrResult rs = xrCreateActionSet(instance, &createInfo, &m_ActionSet);
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

void InputManager::OculusTouch::HapticsThread(XrSession session, OculusTouch* device)
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
				XrHapticActionInfo info = XR_TYPE(HAPTIC_ACTION_INFO);
				info.action = device->m_Vibration;
				info.subactionPath = s_SubActionPaths[i];
				XrHapticVibration vibration = XR_TYPE(HAPTIC_VIBRATION);
				vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
				vibration.amplitude = amplitude;
				vibration.duration = (XrDuration)std::chrono::nanoseconds(freq).count();
				XrResult rs = xrApplyHapticFeedback(session, &info, (XrHapticBaseHeader*)&vibration);
				assert(XR_SUCCEEDED(rs));
			}
		}

		std::this_thread::sleep_for(freq);
	}
}

InputManager::OculusTouch::OculusTouch(XrInstance instance)
	: InputDevice(instance, "touch", "Oculus Touch")
	, m_bHapticsRunning(false)
	, m_Button_Enter(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "enter-click", "Menu button")
	, m_Button_Home(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "home-click", "Home button")
	, m_Button_AX(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "ax-click", "A/X pressed", true)
	, m_Button_BY(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "by-click", "B/Y pressed", true)
	, m_Button_Thumb(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumb-click", "Thumbstick pressed", true)
	, m_Touch_AX(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "ax-touch", "A/X touched", true)
	, m_Touch_BY(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "by-touch", "B/Y touched", true)
	, m_Touch_Thumb(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumb-touch", "Thumbstick touched", true)
	, m_Touch_ThumbRest(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "rest-touch", "Thumb rest touched", true)
	, m_Touch_IndexTrigger(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "trigger-touch", "Trigger touched", true)
	, m_IndexTrigger(this, XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger button", true)
	, m_HandTrigger(this, XR_ACTION_TYPE_FLOAT_INPUT, "grip", "Grip button", true)
	, m_Thumbstick(this, XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick", "Thumbstick", true)
	, m_Pose(this, XR_ACTION_TYPE_POSE_INPUT, "pose", "Controller pose", true)
	, m_Vibration(this, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibration", "Vibration", true)
{
	if (Runtime::Get().UseHack(Runtime::HACK_WMR_PROFILE))
		m_Trackpad_Buttons = Action(this, XR_ACTION_TYPE_FLOAT_INPUT, "trackpad-buttons", "Trackpad Buttons", true);
}

InputManager::OculusTouch::~OculusTouch()
{
	m_bHapticsRunning = false;
	if (m_HapticsThread.joinable())
		m_HapticsThread.join();
}

void InputManager::OculusTouch::StartHaptics(XrSession session)
{
	if (m_bHapticsRunning)
		return;

	m_bHapticsRunning = true;
	m_HapticsThread = std::thread(HapticsThread, session, this);
}

XrPath InputManager::OculusTouch::GetSuggestedBindings(std::vector<XrActionSuggestedBinding>& outBindings) const
{
	std::string prefixes[ovrHand_Count] = { "/user/hand/left", "/user/hand/right" };

#define ADD_BINDING(action, path) outBindings.push_back(XrActionSuggestedBinding{ action, GetXrPath(path) })

	if (Runtime::Get().UseHack(Runtime::HACK_WMR_PROFILE))
	{
		ADD_BINDING(m_Button_Enter, "/user/hand/left/input/menu/click");
		ADD_BINDING(m_Button_Home, "/user/hand/right/input/menu/click");
	}
	else
	{
		if (Runtime::Get().UseHack(Runtime::HACK_VALVE_INDEX_PROFILE))
		{
			ADD_BINDING(m_Button_Enter, "/user/hand/left/input/trackpad/force");
			ADD_BINDING(m_Button_AX, "/user/hand/left/input/a/click");
			ADD_BINDING(m_Button_BY, "/user/hand/left/input/b/click");
			ADD_BINDING(m_Touch_AX, "/user/hand/left/input/a/touch");
			ADD_BINDING(m_Touch_BY, "/user/hand/left/input/b/touch");
			ADD_BINDING(m_Button_Home, "/user/hand/right/input/trackpad/force");
		}
		else
		{
			ADD_BINDING(m_Button_Enter, "/user/hand/left/input/menu/click");
			ADD_BINDING(m_Button_AX, "/user/hand/left/input/x/click");
			ADD_BINDING(m_Button_BY, "/user/hand/left/input/y/click");
			ADD_BINDING(m_Touch_AX, "/user/hand/left/input/x/touch");
			ADD_BINDING(m_Touch_BY, "/user/hand/left/input/y/touch");
			ADD_BINDING(m_Button_Home, "/user/hand/right/input/system/click");
		}
		ADD_BINDING(m_Button_AX, "/user/hand/right/input/a/click");
		ADD_BINDING(m_Button_BY, "/user/hand/right/input/b/click");
		ADD_BINDING(m_Touch_AX, "/user/hand/right/input/a/touch");
		ADD_BINDING(m_Touch_BY, "/user/hand/right/input/b/touch");
	}

	for (int i = 0; i < ovrHand_Count; i++)
	{
		if (Runtime::Get().UseHack(Runtime::HACK_WMR_PROFILE))
		{
			ADD_BINDING(m_Trackpad_Buttons, prefixes[i] + "/input/trackpad/y");
			ADD_BINDING(m_Button_AX, prefixes[i] + "/input/trackpad/click");
			ADD_BINDING(m_Button_BY, prefixes[i] + "/input/trackpad/click");
			ADD_BINDING(m_Touch_AX, prefixes[i] + "/input/trackpad/touch");
			ADD_BINDING(m_Touch_BY, prefixes[i] + "/input/trackpad/touch");

			ADD_BINDING(m_Thumbstick, prefixes[i] + "/input/thumbstick");
			ADD_BINDING(m_Button_Thumb, prefixes[i] + "/input/thumbstick/click");
			ADD_BINDING(m_HandTrigger, prefixes[i] + "/input/squeeze/click");
			ADD_BINDING(m_Touch_IndexTrigger, prefixes[i] + "/input/trigger/value");
			ADD_BINDING(m_IndexTrigger, prefixes[i] + "/input/trigger/value");
		}
		else
		{
			ADD_BINDING(m_Thumbstick, prefixes[i] + "/input/thumbstick");
			ADD_BINDING(m_Button_Thumb, prefixes[i] + "/input/thumbstick/click");
			ADD_BINDING(m_Touch_Thumb, prefixes[i] + "/input/thumbstick/touch");
			if (Runtime::Get().UseHack(Runtime::HACK_VALVE_INDEX_PROFILE))
				ADD_BINDING(m_Touch_ThumbRest, prefixes[i] + "/input/trackpad/touch");
			else
				ADD_BINDING(m_Touch_ThumbRest, prefixes[i] + "/input/thumbrest/touch");
			ADD_BINDING(m_HandTrigger, prefixes[i] + "/input/squeeze/value");
			ADD_BINDING(m_Touch_IndexTrigger, prefixes[i] + "/input/trigger/touch");
			ADD_BINDING(m_IndexTrigger, prefixes[i] + "/input/trigger/value");
		}

		ADD_BINDING(m_Pose, prefixes[i] + "/input/aim/pose");
		ADD_BINDING(m_Vibration, prefixes[i] + "/output/haptic");
	}

#undef ADD_BINDING

	if (Runtime::Get().UseHack(Runtime::HACK_VALVE_INDEX_PROFILE))
		return GetXrPath("/interaction_profiles/valve/index_controller");
	else if (Runtime::Get().UseHack(Runtime::HACK_WMR_PROFILE))
		return GetXrPath("/interaction_profiles/microsoft/motion_controller");
	else
		return GetXrPath("/interaction_profiles/oculus/touch_controller");
}

void InputManager::OculusTouch::GetActionSpaces(XrSession session, std::vector<XrSpace>& outSpaces) const
{
	outSpaces.push_back(m_Pose.CreateSpace(session, ovrHand_Left));
	outSpaces.push_back(m_Pose.CreateSpace(session, ovrHand_Right));
}

void InputManager::OculusTouch::GetActiveSets(std::vector<XrActiveActionSet>& outSets) const
{
	outSets.push_back(XrActiveActionSet{ m_ActionSet, s_SubActionPaths[ovrHand_Left] });
	outSets.push_back(XrActiveActionSet{ m_ActionSet, s_SubActionPaths[ovrHand_Right] });
}

ovrControllerType InputManager::OculusTouch::GetType() const
{
	return ovrControllerType_Touch;
}

bool InputManager::OculusTouch::IsConnected() const
{
	return true;
}

bool InputManager::OculusTouch::GetInputState(XrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	if (m_Button_Enter.GetDigital(session))
		inputState->Buttons |= ovrButton_Enter;

	// In most games this doesn't really do anything,
	// because it would normally open the dashboard.
	if (m_Button_Home.GetDigital(session))
		inputState->Buttons |= ovrButton_Home;

	for (int i = 0; i < ovrHand_Count; i++)
	{
		ovrHandType hand = (ovrHandType)i;
		unsigned int buttons = 0, touches = 0;

		if (Runtime::Get().UseHack(Runtime::HACK_WMR_PROFILE))
		{
			if (m_Button_AX.GetDigital(session, hand) || m_Button_BY.GetDigital(session, hand))
			{
				if (m_Trackpad_Buttons.GetAnalog(session, hand) > 0.0f)
					buttons |= ovrButton_B;
				else
					buttons |= ovrButton_A;
			}

			if (m_Touch_AX.GetDigital(session, hand) || m_Touch_BY.GetDigital(session, hand))
			{
				if (m_Trackpad_Buttons.GetAnalog(session, hand) > 0.0f)
					touches |= ovrTouch_B;
				else
					touches |= ovrTouch_A;
			}
		}
		else
		{
			if (m_Button_AX.GetDigital(session, hand))
				buttons |= ovrButton_A;

			if (m_Touch_AX.GetDigital(session, hand))
				touches |= ovrTouch_A;

			if (m_Button_BY.GetDigital(session, hand))
				buttons |= ovrButton_B;

			if (m_Touch_BY.GetDigital(session, hand))
				touches |= ovrTouch_B;
		}

		if (m_Button_Thumb.GetDigital(session, hand))
			buttons |= ovrButton_RThumb;

		if (m_Touch_Thumb.GetDigital(session, hand))
			touches |= ovrTouch_RThumb;

		if (m_Touch_ThumbRest.GetDigital(session, hand))
			touches |= ovrTouch_RThumbRest;

		if (m_Touch_IndexTrigger.GetDigital(session, hand))
			touches |= ovrTouch_RIndexTrigger;

		inputState->ThumbstickNoDeadzone[i] = m_Thumbstick.GetVector(session, hand);
		inputState->IndexTriggerNoDeadzone[i] = m_IndexTrigger.GetAnalog(session, hand);
		inputState->HandTriggerNoDeadzone[i] = m_HandTrigger.GetAnalog(session, hand);

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

ovrResult InputManager::OculusTouch::SetVibration(XrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	std::vector<XrPath> subPaths;
	if (controllerType & ovrControllerType_LTouch)
		subPaths.push_back(s_SubActionPaths[ovrHand_Left]);
	if (controllerType & ovrControllerType_RTouch)
		subPaths.push_back(s_SubActionPaths[ovrHand_Right]);

	for (XrPath path : subPaths)
	{
		XrHapticActionInfo info = XR_TYPE(HAPTIC_ACTION_INFO);
		info.action = m_Vibration;
		info.subactionPath = path;
		if (frequency > 0.0f)
		{
			XrHapticVibration vibration = XR_TYPE(HAPTIC_VIBRATION);
			vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
			vibration.amplitude = amplitude;
			vibration.duration = 2500000000UL;
			CHK_XR(xrApplyHapticFeedback(session, &info, (XrHapticBaseHeader*)&vibration));
		}
		else
		{
			CHK_XR(xrStopHapticFeedback(session, &info));
		}
	}
	return ovrSuccess;
}

void InputManager::OculusTouch::SubmitVibration(ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	if (controllerType & ovrControllerType_LTouch)
		m_HapticsBuffer[ovrHand_Left].AddSamples(buffer);
	if (controllerType & ovrControllerType_RTouch)
		m_HapticsBuffer[ovrHand_Right].AddSamples(buffer);
}

InputManager::OculusRemote::OculusRemote(XrInstance instance)
	: InputDevice(instance, "remote", "Oculus Remote")
	, m_IsConnected(false)
	, m_Toggle_Connected(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "toggle-connect", "Connect the remote")
	, m_Button_Up(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "up-click", "Up pressed")
	, m_Button_Down(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "down-click", "Down pressed")
	, m_Button_Left(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "left-click", "Left pressed")
	, m_Button_Right(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "right-click", "Right pressed")
	, m_Button_Enter(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "enter-click", "Select pressed")
	, m_Button_Back(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "back-click", "Back pressed")
	, m_Button_VolUp(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "vol-up", "Volume up")
	, m_Button_VolDown(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "vol-down", "Volume down")
{
}

bool InputManager::OculusRemote::IsConnected() const
{
	return m_IsConnected;
}

bool InputManager::OculusRemote::GetInputState(XrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	unsigned int buttons;

	// Allow the user to enable/disable the remote
	if (m_Toggle_Connected.IsPressed(session))
		m_IsConnected = !m_IsConnected;

	if (m_Button_Up.GetDigital(session))
		buttons |= ovrButton_Up;

	if (m_Button_Down.GetDigital(session))
		buttons |= ovrButton_Down;

	if (m_Button_Left.GetDigital(session))
		buttons |= ovrButton_Left;

	if (m_Button_Right.GetDigital(session))
		buttons |= ovrButton_Right;

	if (m_Button_Enter.GetDigital(session))
		buttons |= ovrButton_Enter;

	if (m_Button_Back.GetDigital(session))
		buttons |= ovrButton_Back;

	if (m_Button_VolUp.GetDigital(session))
		buttons |= ovrButton_VolUp;

	if (m_Button_VolDown.GetDigital(session))
		buttons |= ovrButton_VolDown;

	inputState->Buttons |= buttons;
	return true;
}

void InputManager::OculusRemote::GetActiveSets(std::vector<XrActiveActionSet>& outSets) const
{
	XrActiveActionSet active = { m_ActionSet };
	outSets.push_back(active);
}

InputManager::XboxGamepad::XboxGamepad(XrInstance instance)
	: InputDevice(instance, "xbox", "Xbox Controller")
	, m_Button_A(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "a-click", "A pressed")
	, m_Button_B(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "b-click", "B pressed")
	, m_Button_RThumb(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "rthumb-click", "Right thumbstick pressed")
	, m_Button_RShoulder(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "rshoulder-click", "Right shoulder pressed")
	, m_Button_X(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "x-click", "X pressed")
	, m_Button_Y(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "y-click", "Y pressed")
	, m_Button_LThumb(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "lthumb-click", "Left thumbstick pressed")
	, m_Button_LShoulder(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "lshoulder-click", "Left shoulder pressed")
	, m_Button_Up(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "up-click", "Up pressed")
	, m_Button_Down(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "down-click", "Down pressed")
	, m_Button_Left(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "left-click", "Left pressed")
	, m_Button_Right(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "right-click", "Right pressed")
	, m_Button_Enter(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "enter-click", "Select pressed")
	, m_Button_Back(this, XR_ACTION_TYPE_BOOLEAN_INPUT, "back-click", "Back pressed")
	, m_RIndexTrigger(this, XR_ACTION_TYPE_FLOAT_INPUT, "rtrigger", "Right trigger")
	, m_LIndexTrigger(this, XR_ACTION_TYPE_FLOAT_INPUT, "ltrigger", "Left trigger")
	, m_RThumbstick(this, XR_ACTION_TYPE_VECTOR2F_INPUT, "rthumb", "Right thumbstick")
	, m_LThumbstick(this, XR_ACTION_TYPE_VECTOR2F_INPUT, "lthumb", "Left thumbstick")
	, m_RVibration(this, XR_ACTION_TYPE_VIBRATION_OUTPUT, "rvibration", "Right vibration motor")
	, m_LVibration(this, XR_ACTION_TYPE_VIBRATION_OUTPUT, "lvibration", "Left vibration motor")
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

bool InputManager::XboxGamepad::GetInputState(XrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	unsigned int buttons = 0;

	if (m_Button_A.GetDigital(session))
		buttons |= ovrButton_A;

	if (m_Button_B.GetDigital(session))
		buttons |= ovrButton_B;

	if (m_Button_RThumb.GetDigital(session))
		buttons |= ovrButton_RThumb;

	if (m_Button_RShoulder.GetDigital(session))
		buttons |= ovrButton_RShoulder;

	if (m_Button_X.GetDigital(session))
		buttons |= ovrButton_X;

	if (m_Button_Y.GetDigital(session))
		buttons |= ovrButton_Y;

	if (m_Button_LThumb.GetDigital(session))
		buttons |= ovrButton_LThumb;

	if (m_Button_LShoulder.GetDigital(session))
		buttons |= ovrButton_LShoulder;

	if (m_Button_Up.GetDigital(session))
		buttons |= ovrButton_Up;

	if (m_Button_Down.GetDigital(session))
		buttons |= ovrButton_Down;

	if (m_Button_Left.GetDigital(session))
		buttons |= ovrButton_Left;

	if (m_Button_Right.GetDigital(session))
		buttons |= ovrButton_Right;

	if (m_Button_Enter.GetDigital(session))
		buttons |= ovrButton_Enter;

	if (m_Button_Back.GetDigital(session))
		buttons |= ovrButton_Back;

	const Action* triggers[] = { &m_LIndexTrigger, &m_RIndexTrigger };
	const Action* sticks[] = { &m_LThumbstick, &m_RThumbstick };
	float deadzone[] = { 0.24f, 0.265f };
	for (int hand = 0; hand < ovrHand_Count; hand++)
	{
		ovrVector2f thumbstick = sticks[hand]->GetVector(session);
		inputState->IndexTrigger[hand] = triggers[hand]->GetAnalog(session);
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

ovrResult InputManager::XboxGamepad::SetVibration(XrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	XrHapticActionInfo info = XR_TYPE(HAPTIC_ACTION_INFO);
	info.action = frequency > 0.5 ? m_RVibration : m_LVibration;
	XrHapticVibration vibration = XR_TYPE(HAPTIC_VIBRATION);
	vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
	vibration.amplitude = amplitude;
	vibration.duration = 2500000000UL;
	CHK_XR(xrApplyHapticFeedback(session, &info, (XrHapticBaseHeader*)&vibration));
	return ovrSuccess;
}

void InputManager::XboxGamepad::GetActiveSets(std::vector<XrActiveActionSet>& outSets) const
{
	outSets.push_back(XrActiveActionSet{ m_ActionSet });
}

ovrResult InputManager::AttachSession(XrSession session)
{
	for (XrSpace space : m_ActionSpaces)
		CHK_XR(xrDestroySpace(space));
	m_ActionSpaces.clear();

	if (session)
	{
		std::vector<XrActionSet> actionSets;
		for (InputDevice* device : m_InputDevices)
		{
			device->GetActionSpaces(session, m_ActionSpaces);
			actionSets.push_back(device->ActionSet());
		}

		XrSessionActionSetsAttachInfo attachInfo = XR_TYPE(SESSION_ACTION_SETS_ATTACH_INFO);
		attachInfo.countActionSets = (uint32_t)actionSets.size();
		attachInfo.actionSets = actionSets.data();
		CHK_XR(xrAttachSessionActionSets(session, &attachInfo));
	}
	return ovrSuccess;
}
