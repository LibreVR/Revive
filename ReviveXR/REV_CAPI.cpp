#include "OVR_CAPI.h"
#include "OVR_Version.h"
#include "XR_Math.h"

#include "version.h"

#include "Common.h"
#include "Session.h"
#include "Runtime.h"
#include "InputManager.h"
#include "Swapchain.h"

#include <Windows.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <list>
#include <vector>
#include <algorithm>
#include <shared_mutex>
#include <thread>
#include <chrono>
#include <detours/detours.h>

XrInstance g_Instance = XR_NULL_HANDLE;
std::list<ovrHmdStruct> g_Sessions;

using namespace std::chrono_literals;

bool LoadRenderDoc()
{
	LONG error = ERROR_SUCCESS;

	// Open the libraries key
	char keyPath[MAX_PATH] = { "RenderDoc.RDCCapture.1\\DefaultIcon" };
	HKEY iconKey;
	error = RegOpenKeyExA(HKEY_CLASSES_ROOT, keyPath, 0, KEY_READ, &iconKey);
	if (error != ERROR_SUCCESS)
		return false;

	// Get the default library
	char path[MAX_PATH];
	DWORD length = MAX_PATH;
	error = RegQueryValueExA(iconKey, "", NULL, NULL, (PBYTE)path, &length);
	RegCloseKey(iconKey);
	if (error != ERROR_SUCCESS)
		return false;

	if (path[0] == '\0')
		return false;

	strcpy(strrchr(path, '\\') + 1, "renderdoc.dll");
	return LoadLibraryA(path) != NULL;
}

void AttachDetours();
void DetachDetours();

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params)
{
	if (g_Instance)
		return ovrSuccess;

#if 0
	LoadRenderDoc();
#endif

	MicroProfileOnThreadCreate("Main");
	MicroProfileSetForceEnable(true);
	MicroProfileSetEnableAllGroups(true);
	MicroProfileSetForceMetaCounters(true);
	MicroProfileWebServerStart();

	DetachDetours();
	ovrResult rs = Runtime::Get().CreateInstance(&g_Instance, params);
	AttachDetours();
	return rs;
}

OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{
	REV_TRACE(ovr_Shutdown);

	// End all sessions
	std::vector<XrSession> ToDestroy;
	auto it = g_Sessions.begin();
	while (it != g_Sessions.end())
	{
		// After years of work I have perfected my most unmaintainable line of
		// code. It's very important that the iterator is incremented after the
		// pointer is taken but before ovr_Destroy() is called or we *will* crash.
		ovr_Destroy(&*it++);
	}

	// Destroy and reset the instance
	XrResult rs = xrDestroyInstance(g_Instance);
	assert(XR_SUCCEEDED(rs));
	g_Instance = XR_NULL_HANDLE;

	MicroProfileShutdown();
}

OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo)
{
	REV_TRACE(ovr_GetLastErrorInfo);

	if (!errorInfo)
		return;

	xrResultToString(g_Instance, g_LastResult, errorInfo->ErrorString);
	errorInfo->Result = ResultToOvrResult(g_LastResult);
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString()
{
	REV_TRACE(ovr_GetVersionString);

	return OVR_VERSION_STRING;
}

OVR_PUBLIC_FUNCTION(int) ovr_TraceMessage(int level, const char* message) { return 0; /* Debugging feature */ }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_IdentifyClient(const char* identity) { return ovrSuccess; /* Debugging feature */ }

OVR_PUBLIC_FUNCTION(ovrHmdDesc) ovr_GetHmdDesc(ovrSession session)
{
	REV_TRACE(ovr_GetHmdDesc);

	ovrHmdDesc desc = { ovrHmd_None };

	if (!g_Instance)
		return desc;

	XrSystemId System;
	XrSystemGetInfo systemInfo = XR_TYPE(SYSTEM_GET_INFO);
	systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	if (session || XR_SUCCEEDED(xrGetSystem(g_Instance, &systemInfo, &System)))
		desc.Type = Runtime::Get().MinorVersion < 38 ? ovrHmd_CV1 : ovrHmd_RiftS;

	if (!session)
		return desc;

	XrInstanceProperties props = XR_TYPE(INSTANCE_PROPERTIES);
	xrGetInstanceProperties(session->Instance, &props);

	strcpy_s(desc.ProductName, 64, "Oculus Rift S");
	strcpy_s(desc.Manufacturer, 64, props.runtimeName);

	if (session->SystemProperties.trackingProperties.orientationTracking)
		desc.AvailableTrackingCaps |= ovrTrackingCap_Orientation;
	if (session->SystemProperties.trackingProperties.positionTracking)
		desc.AvailableTrackingCaps |= ovrTrackingCap_Orientation;
	desc.DefaultTrackingCaps = desc.AvailableTrackingCaps;

	for (int i = 0; i < ovrEye_Count; i++)
	{
		// Compensate for the 3-DOF eye pose on pre-1.17
		if (Runtime::Get().MinorVersion < 17)
		{
			desc.DefaultEyeFov[i] = OVR::FovPort::Uncant(XR::FovPort(session->ViewPoses[i].fov), XR::Quatf(session->ViewPoses[i].pose.orientation));
			desc.MaxEyeFov[i] = desc.DefaultEyeFov[i];
		}
		else
		{
			desc.DefaultEyeFov[i] = XR::FovPort(session->ViewFov[i].recommendedFov);
			desc.MaxEyeFov[i] = XR::FovPort(session->ViewFov[i].maxMutableFov);
		}
		desc.Resolution.w += (int)session->ViewConfigs[i].recommendedImageRectWidth;
		desc.Resolution.h = std::max(desc.Resolution.h, (int)session->ViewConfigs[i].recommendedImageRectHeight);
	}

	XrIndexedFrameState* frame = session->CurrentFrame;
	desc.DisplayRefreshRate = frame->predictedDisplayPeriod > 0 ? 1e9f / frame->predictedDisplayPeriod : 90.0f;
	return desc;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackerCount(ovrSession session)
{
	REV_TRACE(ovr_GetTrackerCount);

	if (!session)
		return ovrError_InvalidSession;

	// Pre-1.37 applications need virtual sensors to avoid a loss of tracking being detected
	return Runtime::Get().MinorVersion < 37 ? 3 : 0;
}

OVR_PUBLIC_FUNCTION(ovrTrackerDesc) ovr_GetTrackerDesc(ovrSession session, unsigned int trackerDescIndex)
{
	REV_TRACE(ovr_GetTrackerDesc);

	ovrTrackerDesc desc = { 0 };
	if (trackerDescIndex < ovr_GetTrackerCount(session))
	{
		desc.FrustumHFovInRadians = OVR::DegreeToRad(100.0f);
		desc.FrustumVFovInRadians = OVR::DegreeToRad(70.0f);
		desc.FrustumNearZInMeters = 0.4f;
		desc.FrustumFarZInMeters = 2.5;
	}
	return desc;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Create(ovrSession* pSession, ovrGraphicsLuid* pLuid)
{
	REV_TRACE(ovr_Create);

	if (!pSession)
		return ovrError_InvalidParameter;

	*pSession = nullptr;

	// Initialize the opaque pointer with our own OpenXR-specific struct
	g_Sessions.emplace_back();
	ovrSession session = &g_Sessions.back();

	// Initialize session, it will not be fully usable until a swapchain is created
	CHK_OVR(session->InitSession(g_Instance));
	if (pLuid)
		*pLuid = session->Adapter;
	*pSession = session;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_Destroy(ovrSession session)
{
	REV_TRACE(ovr_Destroy);

	session->DestroySession();

	if (!session->HookedFunctions.empty())
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		for (auto it : session->HookedFunctions)
		DetourDetach(it.first, it.second);
		DetourTransactionCommit();
	}

	// Delete the session from the list of sessions
	g_Sessions.erase(std::find_if(g_Sessions.begin(), g_Sessions.end(), [session](ovrHmdStruct const& o) { return &o == session; }));
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetSessionStatus(ovrSession session, ovrSessionStatus* sessionStatus)
{
	REV_TRACE(ovr_GetSessionStatus);

	if (!session)
		return ovrError_InvalidSession;

	if (!sessionStatus)
		return ovrError_InvalidParameter;

	assert(session->SessionStatus.is_lock_free());
	SessionStatusBits status = session->SessionStatus;
	XrEventDataBuffer event = XR_TYPE(EVENT_DATA_BUFFER);
	while (XR_UNQUALIFIED_SUCCESS(xrPollEvent(session->Instance, &event)))
	{
		switch (event.type)
		{
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			const XrEventDataSessionStateChanged& stateChanged =
				reinterpret_cast<XrEventDataSessionStateChanged&>(event);
			if (stateChanged.session == session->Session)
			{
				switch (stateChanged.state)
				{
				case XR_SESSION_STATE_IDLE:
					status.HmdPresent = true;
					break;
				case XR_SESSION_STATE_READY:
					// Oculus apps won't synchronize before they're visible,
					// so we have to set the IsVisible flag immediately.
					status.IsVisible = true;
					status.HmdMounted = true;
					break;
				case XR_SESSION_STATE_SYNCHRONIZED:
					break;
				case XR_SESSION_STATE_VISIBLE:
					status.HasInputFocus = false;
					break;
				case XR_SESSION_STATE_FOCUSED:
					status.HasInputFocus = true;
					break;
				case XR_SESSION_STATE_STOPPING:
					status.IsVisible = false;
					status.HmdMounted = false;
					break;
				case XR_SESSION_STATE_LOSS_PENDING:
					status.DisplayLost = true;
					break;
				case XR_SESSION_STATE_EXITING:
					status.ShouldQuit = true;
					break;
				}
				session->SessionStatus = status;

				if (stateChanged.state == XR_SESSION_STATE_READY)
					session->BeginSession();
				if (stateChanged.state == XR_SESSION_STATE_STOPPING)
					session->EndSession();
			}
			break;
		}
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
		{
			const XrEventDataInstanceLossPending& lossPending =
				reinterpret_cast<XrEventDataInstanceLossPending&>(event);
			status.ShouldQuit = true;
			session->SessionStatus = status;
			break;
		}
		case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
		{
			const XrEventDataReferenceSpaceChangePending& spaceChange =
				reinterpret_cast<XrEventDataReferenceSpaceChangePending&>(event);
			status.ShouldRecenter = true;
			session->SessionStatus = status;
			break;
		}
		case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR:
		{
			const XrEventDataVisibilityMaskChangedKHR& maskChange =
				reinterpret_cast<XrEventDataVisibilityMaskChangedKHR&>(event);

			if (maskChange.viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
				&& maskChange.viewIndex < ovrEye_Count)
			{
				session->UpdateStencil((ovrEyeType)maskChange.viewIndex, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR);
				session->UpdateStencil((ovrEyeType)maskChange.viewIndex, XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR);
				session->UpdateStencil((ovrEyeType)maskChange.viewIndex, XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR);
			}
			break;
		}
		}
		event = XR_TYPE(EVENT_DATA_BUFFER);
	}

	sessionStatus->IsVisible = status.IsVisible;
	sessionStatus->HmdPresent = status.HmdPresent;
	sessionStatus->HmdMounted = status.HmdMounted;
	sessionStatus->DisplayLost = status.DisplayLost;
	sessionStatus->ShouldQuit = status.ShouldQuit;
	sessionStatus->ShouldRecenter = status.ShouldRecenter;
	sessionStatus->HasInputFocus = status.HasInputFocus;
	sessionStatus->OverlayPresent = status.OverlayPresent;
	if (Runtime::Get().MinorVersion > 20)
		sessionStatus->DepthRequested = Runtime::Get().CompositionDepth;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetTrackingOriginType(ovrSession session, ovrTrackingOrigin origin)
{
	REV_TRACE(ovr_SetTrackingOriginType);

	if (!session)
		return ovrError_InvalidSession;

	assert(session->TrackingOrigin.is_lock_free());
	session->TrackingOrigin = origin;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrTrackingOrigin) ovr_GetTrackingOriginType(ovrSession session)
{
	REV_TRACE(ovr_GetTrackingOriginType);

	if (!session)
		return ovrTrackingOrigin_EyeLevel;

	return session->TrackingOrigin;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RecenterTrackingOrigin(ovrSession session)
{
	REV_TRACE(ovr_RecenterTrackingOrigin);

	if (!session)
		return ovrError_InvalidSession;

	if (session->ViewSpace == XR_NULL_HANDLE)
		return ovrSuccess;

	ovr_ClearShouldRecenterFlag(session); 
	return session->RecenterSpace(session->TrackingOrigin, session->ViewSpace);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SpecifyTrackingOrigin(ovrSession session, ovrPosef originPose)
{
	if (!session)
		return ovrError_InvalidSession;

	ovr_ClearShouldRecenterFlag(session);
	XrSpace anchor = session->TrackingSpaces[session->TrackingOrigin];
	return session->RecenterSpace(session->TrackingOrigin, anchor, originPose);
}

OVR_PUBLIC_FUNCTION(void) ovr_ClearShouldRecenterFlag(ovrSession session)
{
	if (!session)
		return;

	SessionStatusBits status = session->SessionStatus;
	status.ShouldRecenter = false;
	session->SessionStatus = status;
}

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingState(ovrSession session, double absTime, ovrBool latencyMarker)
{
	REV_TRACE(ovr_GetTrackingState);

	ovrTrackingState state = { 0 };
	if (session && session->Input)
	{
		std::shared_lock<std::shared_mutex> lk(session->TrackingMutex);
		session->Input->GetTrackingState(session, &state, absTime);
	}
	return state;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetDevicePoses(ovrSession session, ovrTrackedDeviceType* deviceTypes, int deviceCount, double absTime, ovrPoseStatef* outDevicePoses)
{
	REV_TRACE(ovr_GetDevicePoses);

	if (!session || !session->Input)
		return ovrError_InvalidSession;

	std::shared_lock<std::shared_mutex> lk(session->TrackingMutex);
	return session->Input->GetDevicePoses(session, deviceTypes, deviceCount, absTime, outDevicePoses);
}

struct ovrSensorData_;
typedef struct ovrSensorData_ ovrSensorData;

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingStateWithSensorData(ovrSession session, double absTime, ovrBool latencyMarker, ovrSensorData* sensorData)
{
	REV_TRACE(ovr_GetTrackingStateWithSensorData);

	// This is a private API, ignore the raw sensor data request and hope for the best.
	assert(sensorData == nullptr);

	return ovr_GetTrackingState(session, absTime, latencyMarker);
}

OVR_PUBLIC_FUNCTION(ovrTrackerPose) ovr_GetTrackerPose(ovrSession session, unsigned int trackerPoseIndex)
{
	REV_TRACE(ovr_GetTrackerPose);

	ovrTrackerPose tracker = { 0 };

	if (!session)
		return tracker;

	if (trackerPoseIndex < ovr_GetTrackerCount(session))
	{
		const OVR::Posef poses[] = {
			OVR::Posef(OVR::Quatf(OVR::Axis_Y, OVR::DegreeToRad(90.0f)), OVR::Vector3f(-2.0f, 0.0f, 0.2f)),
			OVR::Posef(OVR::Quatf(OVR::Axis_Y, OVR::DegreeToRad(0.0f)), OVR::Vector3f(-0.2f, 0.0f, -2.0f)),
			OVR::Posef(OVR::Quatf(OVR::Axis_Y, OVR::DegreeToRad(180.0f)), OVR::Vector3f(0.2f, 0.0f, 2.0f))
		};
		OVR::Posef trackerPose = poses[trackerPoseIndex];

		std::shared_lock<std::shared_mutex> lk(session->TrackingMutex);
		XrSpaceLocation relation = XR_TYPE(SPACE_LOCATION);
		if (session->Session && XR_SUCCEEDED(xrLocateSpace(session->ViewSpace, session->TrackingSpaces[ovrTrackingOrigin_EyeLevel],
			(*session->CurrentFrame).predictedDisplayTime, &relation)))
		{
			// Create a leveled head pose
			if (relation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
			{
				float yaw;
				XR::Posef headPose(relation.pose);
				headPose.Rotation.GetYawPitchRoll(&yaw, nullptr, nullptr);
				headPose.Rotation = OVR::Quatf(OVR::Axis_Y, yaw);

				// Rotate the trackers with the headset so that the headset is always in view
				trackerPose = headPose * trackerPose;
			}
		}

		tracker.Pose = trackerPose;
		tracker.LeveledPose = trackerPose;
		tracker.TrackerFlags = ovrTracker_Connected | ovrTracker_PoseTracked;
	}

	return tracker;
}

// Pre-1.7 input state
typedef struct ovrInputState1_
{
	double              TimeInSeconds;
	unsigned int        Buttons;
	unsigned int        Touches;
	float               IndexTrigger[ovrHand_Count];
	float               HandTrigger[ovrHand_Count];
	ovrVector2f         Thumbstick[ovrHand_Count];
	ovrControllerType   ControllerType;
} ovrInputState1;

// Pre-1.11 input state
typedef struct ovrInputState2_
{
	double              TimeInSeconds;
	unsigned int        Buttons;
	unsigned int        Touches;
	float               IndexTrigger[ovrHand_Count];
	float               HandTrigger[ovrHand_Count];
	ovrVector2f         Thumbstick[ovrHand_Count];
	ovrControllerType   ControllerType;
	float               IndexTriggerNoDeadzone[ovrHand_Count];
	float               HandTriggerNoDeadzone[ovrHand_Count];
	ovrVector2f         ThumbstickNoDeadzone[ovrHand_Count];
} ovrInputState2;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	REV_TRACE(ovr_GetInputState);
	MICROPROFILE_META_CPU("Controller Type", controllerType);

	if (!session)
		return ovrError_InvalidSession;

	if (!inputState)
		return ovrError_InvalidParameter;

	ovrInputState state = { 0 };

	ovrResult result = ovrSuccess;
	if (session->Input && session->Session)
		result = session->Input->GetInputState(session, controllerType, &state);

	// We need to make sure we don't write outside of the bounds of the struct
	// when the client expects a pre-1.7 version of LibOVR.
	if (Runtime::Get().MinorVersion < 7)
		memcpy(inputState, &state, sizeof(ovrInputState1));
	else if (Runtime::Get().MinorVersion < 11)
		memcpy(inputState, &state, sizeof(ovrInputState2));
	else
		memcpy(inputState, &state, sizeof(ovrInputState));

	return result;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetConnectedControllerTypes(ovrSession session)
{
	REV_TRACE(ovr_GetConnectedControllerTypes);

	return ovrControllerType_Touch | ovrControllerType_XBox | ovrControllerType_Remote;
}

OVR_PUBLIC_FUNCTION(ovrTouchHapticsDesc) ovr_GetTouchHapticsDesc(ovrSession session, ovrControllerType controllerType)
{
	REV_TRACE(ovr_GetTouchHapticsDesc);
	MICROPROFILE_META_CPU("Controller Type", controllerType);

	ovrTouchHapticsDesc desc = { 0 };
	if (session && controllerType & ovrControllerType_Touch)
	{
		if ((*session->CurrentFrame).predictedDisplayPeriod == 0)
			desc.SampleRateHz = 0;
		else
			desc.SampleRateHz = (int)(1000000000i64 / (*session->CurrentFrame).predictedDisplayPeriod);
		desc.SampleSizeInBytes = sizeof(uint8_t);
		desc.SubmitMaxSamples = OVR_HAPTICS_BUFFER_SAMPLES_MAX;
		desc.SubmitMinSamples = 1;
		desc.SubmitOptimalSamples = 20;
		desc.QueueMinSizeToAvoidStarvation = 5;
	}
	return desc;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	REV_TRACE(ovr_SetControllerVibration);
	MICROPROFILE_META_CPU("Controller Type", controllerType);

	if (!session || !session->Input)
		return ovrError_InvalidSession;

	return session->Input->SetControllerVibration(session, controllerType, frequency, amplitude);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitControllerVibration(ovrSession session, ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	REV_TRACE(ovr_SubmitControllerVibration);
	MICROPROFILE_META_CPU("Controller Type", controllerType);

	if (!session || !session->Input)
		return ovrError_InvalidSession;

	return session->Input->SubmitControllerVibration(session, controllerType, buffer);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState)
{
	REV_TRACE(ovr_GetControllerVibrationState);
	MICROPROFILE_META_CPU("Controller Type", controllerType);

	if (!session || !session->Input)
		return ovrError_InvalidSession;

	return session->Input->GetControllerVibrationState(session, controllerType, outState);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_TestBoundary(ovrSession session, ovrTrackedDeviceType deviceBitmask,
	ovrBoundaryType boundaryType, ovrBoundaryTestResult* outTestResult)
{
	REV_TRACE(ovr_TestBoundary);

	outTestResult->ClosestDistance = INFINITY;

	std::vector<ovrPoseStatef> poses;
	std::vector<ovrTrackedDeviceType> devices;
	for (uint32_t i = 1; i & ovrTrackedDevice_All; i <<= 1)
	{
		if (i & deviceBitmask)
			devices.push_back((ovrTrackedDeviceType)i);
	}

	poses.resize(devices.size());
	CHK_OVR(ovr_GetDevicePoses(session, devices.data(), (uint32_t)devices.size(), 0.0f, poses.data()));

	for (size_t i = 0; i < devices.size(); i++)
	{
		ovrBoundaryTestResult result = { 0 };
		ovrResult err = ovr_TestBoundaryPoint(session, &poses[i].ThePose.Position, boundaryType, &result);
		if (OVR_SUCCESS(err) && result.ClosestDistance < outTestResult->ClosestDistance)
			*outTestResult = result;
	}
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_TestBoundaryPoint(ovrSession session, const ovrVector3f* point,
	ovrBoundaryType singleBoundaryType, ovrBoundaryTestResult* outTestResult)
{
	REV_TRACE(ovr_TestBoundaryPoint);

	ovrBoundaryTestResult result = { 0 };

	result.IsTriggering = ovrFalse;

	ovrVector3f bounds;
	CHK_OVR(ovr_GetBoundaryDimensions(session, singleBoundaryType, &bounds));

	// Clamp the point to the AABB
	OVR::Vector2f p(point->x, point->z);
	OVR::Vector2f halfExtents(bounds.x / 2.0f, bounds.z / 2.0f);
	OVR::Vector2f clamped = OVR::Vector2f::Min(OVR::Vector2f::Max(p, -halfExtents), halfExtents);

	// If the point is inside the AABB, we need to do some extra work
	if (clamped.Compare(p))
	{
		if (std::abs(p.x) > std::abs(p.y))
			clamped.x = halfExtents.x * (p.x / std::abs(p.x));
		else
			clamped.y = halfExtents.y * (p.y / std::abs(p.y));
	}

	// We don't have a ceiling, use the height from the original point
	result.ClosestPoint.x = clamped.x;
	result.ClosestPoint.y = point->y;
	result.ClosestPoint.z = clamped.y;

	// Get the normal, closest distance is the length of this normal
	OVR::Vector2f normal = p - clamped;
	result.ClosestDistance = normal.Length();

	// Normalize the normal
	normal.Normalize();
	result.ClosestPointNormal.x = normal.x;
	result.ClosestPointNormal.y = 0.0f;
	result.ClosestPointNormal.z = normal.y;

	*outTestResult = result;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetBoundaryLookAndFeel(ovrSession session, const ovrBoundaryLookAndFeel* lookAndFeel)
{
	REV_TRACE(ovr_SetBoundaryLookAndFeel);

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_ResetBoundaryLookAndFeel(ovrSession session)
{
	REV_TRACE(ovr_ResetBoundaryLookAndFeel);

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryGeometry(ovrSession session, ovrBoundaryType boundaryType, ovrVector3f* outFloorPoints, int* outFloorPointsCount)
{
	REV_TRACE(ovr_GetBoundaryGeometry);

	if (!session)
		return ovrError_InvalidSession;

	if (outFloorPoints)
	{
		ovrVector3f bounds;
		CHK_OVR(ovr_GetBoundaryDimensions(session, boundaryType, &bounds));
		ovrVector3f floorPoints[] = {
			{ bounds.x / -2.0f, bounds.y, bounds.z /  2.0f},
			{ bounds.x /  2.0f, bounds.y, bounds.z /  2.0f},
			{ bounds.x /  2.0f, bounds.y, bounds.z / -2.0f},
			{ bounds.x / -2.0f, bounds.y, bounds.z / -2.0f}
		};
		memcpy(outFloorPoints, floorPoints, sizeof(floorPoints));
	}
	if (outFloorPointsCount)
		*outFloorPointsCount = 4;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryDimensions(ovrSession session, ovrBoundaryType boundaryType, ovrVector3f* outDimensions)
{
	REV_TRACE(ovr_GetBoundaryDimensions);

	if (!session)
		return ovrError_InvalidSession;

	if (session->Session == NULL)
	{
		outDimensions->x = session->bounds.width;
		outDimensions->y = 0.0f; // TODO: Find some good default height
		outDimensions->z = session->bounds.height;
		return ovrSuccess;
	}

	XrExtent2Df bounds;
	CHK_XR(xrGetReferenceSpaceBoundsRect(session->Session, XR_REFERENCE_SPACE_TYPE_STAGE, &bounds));

	outDimensions->x = bounds.width;
	outDimensions->y = 0.0f; // TODO: Find some good default height
	outDimensions->z = bounds.height;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryVisible(ovrSession session, ovrBool* outIsVisible)
{
	REV_TRACE(ovr_GetBoundaryVisible);

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RequestBoundaryVisible(ovrSession session, ovrBool visible)
{
	REV_TRACE(ovr_RequestBoundaryVisible);

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainLength(ovrSession session, ovrTextureSwapChain chain, int* out_Length)
{
	REV_TRACE(ovr_GetTextureSwapChainLength);

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", PtrToInt(chain->Swapchain));
	*out_Length = chain->Length;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainCurrentIndex(ovrSession session, ovrTextureSwapChain chain, int* out_Index)
{
	REV_TRACE(ovr_GetTextureSwapChainCurrentIndex);

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", PtrToInt(chain->Swapchain));
	MICROPROFILE_META_CPU("Index", chain->CurrentIndex);
	*out_Index = chain->CurrentIndex;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainDesc(ovrSession session, ovrTextureSwapChain chain, ovrTextureSwapChainDesc* out_Desc)
{
	REV_TRACE(ovr_GetTextureSwapChainDesc);

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", PtrToInt(chain->Swapchain));
	*out_Desc = chain->Desc;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CommitTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	REV_TRACE(ovr_CommitTextureSwapChain);

	if (!session)
		return ovrError_InvalidSession;

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", PtrToInt(chain->Swapchain));
	MICROPROFILE_META_CPU("CurrentIndex", chain->CurrentIndex);

	CHK_OVR(chain->Commit(session));

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	REV_TRACE(ovr_DestroyTextureSwapChain);

	if (!chain)
		return;

	XrResult rs = xrDestroySwapchain(chain->Swapchain);
	assert(XR_SUCCEEDED(rs));
	delete[] chain->Images;
	delete chain;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyMirrorTexture(ovrSession session, ovrMirrorTexture mirrorTexture)
{
	REV_TRACE(ovr_DestroyMirrorTexture);

	if (!mirrorTexture)
		return;

	ovr_DestroyTextureSwapChain(session, mirrorTexture->Dummy);
	delete mirrorTexture;
}

OVR_PUBLIC_FUNCTION(ovrSizei) ovr_GetFovTextureSize(ovrSession session, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel)
{
	REV_TRACE(ovr_GetFovTextureSize);

	// TODO: Add support for pixelsPerDisplayPixel
	ovrSizei size = {
		(int)(session->PixelsPerTan[eye].x * (fov.LeftTan + fov.RightTan)),
		(int)(session->PixelsPerTan[eye].y * (fov.UpTan + fov.DownTan)),
	};
	return size;
}

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovr_GetRenderDesc2(ovrSession session, ovrEyeType eyeType, ovrFovPort fov)
{
	REV_TRACE(ovr_GetRenderDesc);

	if (!session)
		return ovrEyeRenderDesc();

	ovrEyeRenderDesc desc = { eyeType, fov };

	for (int i = 0; i < eyeType; i++)
		desc.DistortedViewport.Pos.x += session->ViewConfigs[i].recommendedImageRectWidth;

	desc.DistortedViewport.Size.w = session->ViewConfigs[eyeType].recommendedImageRectWidth;
	desc.DistortedViewport.Size.h = session->ViewConfigs[eyeType].recommendedImageRectHeight;
	desc.PixelsPerTanAngleAtCenter = session->PixelsPerTan[eyeType];
	desc.HmdToEyePose = XR::Posef(session->ViewPoses[eyeType].pose);
	return desc;
}

typedef struct OVR_ALIGNAS(4) ovrEyeRenderDesc1_ {
	ovrEyeType Eye;
	ovrFovPort Fov;
	ovrRecti DistortedViewport;
	ovrVector2f PixelsPerTanAngleAtCenter;
	ovrVector3f HmdToEyeOffset;
} ovrEyeRenderDesc1;

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc1) ovr_GetRenderDesc(ovrSession session, ovrEyeType eyeType, ovrFovPort fov)
{
	ovrEyeRenderDesc1 legacy = {};
	ovrEyeRenderDesc desc = ovr_GetRenderDesc2(session, eyeType, fov);
	memcpy(&legacy, &desc, sizeof(ovrEyeRenderDesc1));
	legacy.HmdToEyeOffset = desc.HmdToEyePose.Position;
	return legacy;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_WaitToBeginFrame(ovrSession session, long long frameIndex)
{
	REV_TRACE(ovr_WaitToBeginFrame);
	MICROPROFILE_META_CPU("Wait Frame", (int)frameIndex);

	if (!session)
		return ovrError_InvalidSession;

	SessionStatusBits status = session->SessionStatus;
	if (!status.IsVisible)
		return ovrSuccess_NotVisible;

	{
		// Wait until the session is running, since the render thread may still be initializing
		std::unique_lock<std::mutex> lk(session->Running.first);
		if (!session->Running.second.wait_for(lk, 10s, [session] { return session->Session != XR_NULL_HANDLE; }))
			return ovrError_Timeout;
	}

	assert(session->CurrentFrame.is_lock_free());
	XrIndexedFrameState* frameState = &session->FrameStats[frameIndex % ovrMaxProvidedFrameStats];
	XrFrameWaitInfo waitInfo = XR_TYPE(FRAME_WAIT_INFO);
	CHK_XR(xrWaitFrame(session->Session, &waitInfo, frameState));
	frameState->frameIndex = frameIndex;
	session->CurrentFrame = frameState;

	if (session->Input)
		session->Input->SyncInputState(session->Session, frameState->predictedDisplayPeriod);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_BeginFrame(ovrSession session, long long frameIndex)
{
	REV_TRACE(ovr_BeginFrame);
	MICROPROFILE_META_CPU("Begin Frame", (int)frameIndex);

	if (!session)
		return ovrError_InvalidSession;

	SessionStatusBits status = session->SessionStatus;
	if (!status.IsVisible)
		return ovrSuccess_NotVisible;

	assert(frameIndex == (*session->CurrentFrame).frameIndex);

	XrFrameBeginInfo beginInfo = XR_TYPE(FRAME_BEGIN_INFO);
	CHK_XR(xrBeginFrame(session->Session, &beginInfo));
	return ovrSuccess;
}

union XrCompositionLayerUnion
{
	XrCompositionLayerBaseHeader Header;
	XrCompositionLayerProjection Projection;
	XrCompositionLayerQuad Quad;
	XrCompositionLayerCylinderKHR Cylinder;
	XrCompositionLayerCubeKHR Cube;
};

struct XrCompositionLayerProjectionViewStereo
{
	XrCompositionLayerProjectionView Views[ovrEye_Count];
};

OVR_PUBLIC_FUNCTION(ovrResult) ovr_EndFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	REV_TRACE(ovr_EndFrame);
	MICROPROFILE_META_CPU("End Frame", (int)frameIndex);

	if (!session)
		return ovrError_InvalidSession;

	SessionStatusBits status = session->SessionStatus;
	if (!status.IsVisible)
		return ovrSuccess_NotVisible;

	// We are going to use some space handles here, don't destroy them
	std::shared_lock<std::shared_mutex> lk(session->TrackingMutex);

	// The oculus runtime is very tolerant of invalid viewports, so this lambda ensures we submit valid ones.
	// This fixes UE4 games which can at times submit uninitialized viewports due to a bug in OVR_Math.h.
	const auto ClampRect = [](ovrRecti rect, ovrTextureSwapChain chain)
	{
		OVR::Sizei chainSize(chain->Desc.Width, chain->Desc.Height);

		// Clamp the rectangle size within the chain size
		rect.Size = OVR::Sizei::Min(OVR::Sizei::Max(rect.Size, OVR::Sizei(1, 1)), chainSize);

		// Set any invalid coordinates to zero
		if (rect.Pos.x < 0 || rect.Pos.x + rect.Size.w > chainSize.w)
			rect.Pos.x = 0;
		if (rect.Pos.y < 0 || rect.Pos.y + rect.Size.h > chainSize.h)
			rect.Pos.y = 0;

		return XR::Recti(rect);
	};

	std::vector<XrCompositionLayerBaseHeader*> layers;
	std::list<XrCompositionLayerUnion> layerData;
	std::list<XrCompositionLayerProjectionViewStereo> viewData;
	std::list<XrCompositionLayerDepthInfoKHR> depthData;
	for (unsigned int i = 0; i < layerCount; i++)
	{
		ovrLayer_Union* layer = (ovrLayer_Union*)layerPtrList[i];

		if (!layer)
			continue;

		ovrLayerType type = layer->Header.Type;
		const bool upsideDown = layer->Header.Flags & ovrLayerFlag_TextureOriginAtBottomLeft;
		const bool headLocked = layer->Header.Flags & ovrLayerFlag_HeadLocked;

		// Version 1.25 introduced a 128-byte reserved parameter, so on older versions the actual data
		// falls within this reserved parameter and we need to move the pointer back into the actual data area.
		// NOTE: Do not read the header after this operation as it will fall outside of the layer memory.
		if (Runtime::Get().MinorVersion < 25)
			layer = (ovrLayer_Union*)((char*)layer - sizeof(ovrLayerHeader::Reserved));

		if (type == ovrLayerType_Disabled)
			continue;

		layerData.emplace_back();
		XrCompositionLayerUnion& newLayer = layerData.back();

		if (type == ovrLayerType_EyeFov || type == ovrLayerType_EyeMatrix || type == ovrLayerType_EyeFovDepth)
		{
			XrCompositionLayerProjection& projection = newLayer.Projection;
			projection = XR_TYPE(COMPOSITION_LAYER_PROJECTION);

			ovrTextureSwapChain texture = nullptr;
			viewData.emplace_back();
			int i;
			for (i = 0; i < ovrEye_Count; i++)
			{
				if (layer->EyeFov.ColorTexture[i])
					texture = layer->EyeFov.ColorTexture[i];

				if (!texture)
					break;

				XrCompositionLayerProjectionView& view = viewData.back().Views[i];
				view = XR_TYPE(COMPOSITION_LAYER_PROJECTION_VIEW);

				if (type == ovrLayerType_EyeMatrix)
				{
					// RenderPose is the first member that's differently aligned
					view.pose = XR::Posef(layer->EyeMatrix.RenderPose[i]);
					view.fov = XR::Matrix4f(layer->EyeMatrix.Matrix[i]);
				}
				else
				{
					view.pose = XR::Posef(layer->EyeFov.RenderPose[i]);

					// The Climb specifies an invalid fov in the first frame, ignore the layer
					XR::FovPort Fov(layer->EyeFov.Fov[i]);
					if (Fov.GetMaxSideTan() > 0.0f)
						view.fov = Fov;
					else
						break;
				}

				// Flip the field-of-view to flip the image, invert the check for OpenGL
				if (texture->Images->type == XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR ? !upsideDown : upsideDown)
					OVR::OVRMath_Swap(view.fov.angleUp, view.fov.angleDown);

				if (type == ovrLayerType_EyeFovDepth && Runtime::Get().CompositionDepth)
				{
					depthData.emplace_back();
					XrCompositionLayerDepthInfoKHR& depthInfo = depthData.back();
					depthInfo = XR_TYPE(COMPOSITION_LAYER_DEPTH_INFO_KHR);

					ovrTextureSwapChain depthTexture = layer->EyeFovDepth.DepthTexture[i];
					depthInfo.subImage.swapchain = depthTexture->Swapchain;
					depthInfo.subImage.imageRect = ClampRect(layer->EyeFovDepth.Viewport[i], depthTexture);
					depthInfo.subImage.imageArrayIndex = 0;

					const ovrTimewarpProjectionDesc& projDesc = layer->EyeFovDepth.ProjectionDesc;
					depthInfo.minDepth = 0.0f;
					depthInfo.maxDepth = 1.0f;
					depthInfo.nearZ = projDesc.Projection23 / projDesc.Projection22;
					depthInfo.farZ = projDesc.Projection23 / (1.0f + projDesc.Projection22);

					if (viewScaleDesc)
					{
						depthInfo.nearZ *= viewScaleDesc->HmdSpaceToWorldScaleInMeters;
						depthInfo.farZ *= viewScaleDesc->HmdSpaceToWorldScaleInMeters;
					}

					view.next = &depthData.back();
				}

				view.subImage.swapchain = texture->Swapchain;
				view.subImage.imageRect = ClampRect(layer->EyeFov.Viewport[i], texture);
				view.subImage.imageArrayIndex = 0;
			}

			// Verify all views were initialized without errors, otherwise ignore the layer
			if (i < ovrEye_Count)
				continue;

			projection.viewCount = ovrEye_Count;
			projection.views = reinterpret_cast<XrCompositionLayerProjectionView*>(&viewData.back());
		}
		else if (type == ovrLayerType_Quad)
		{
			ovrTextureSwapChain texture = layer->Quad.ColorTexture;
			if (!texture)
				continue;

			XrCompositionLayerQuad& quad = newLayer.Quad;
			quad = XR_TYPE(COMPOSITION_LAYER_QUAD);
			quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
			quad.subImage.swapchain = texture->Swapchain;
			quad.subImage.imageRect = ClampRect(layer->Quad.Viewport, texture);
			quad.subImage.imageArrayIndex = 0;
			quad.pose = XR::Posef(layer->Quad.QuadPoseCenter);
			quad.size = XR::Vector2f(layer->Quad.QuadSize);
		}
		else if (type == ovrLayerType_Cylinder && Runtime::Get().CompositionCylinder)
		{
			ovrTextureSwapChain texture = layer->Cylinder.ColorTexture;
			if (!texture)
				continue;

			XrCompositionLayerCylinderKHR& cylinder = newLayer.Cylinder;
			cylinder = XR_TYPE(COMPOSITION_LAYER_CYLINDER_KHR);
			cylinder.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
			cylinder.subImage.swapchain = texture->Swapchain;
			cylinder.subImage.imageRect = ClampRect(layer->Cylinder.Viewport, texture);
			cylinder.subImage.imageArrayIndex = 0;
			cylinder.pose = XR::Posef(layer->Cylinder.CylinderPoseCenter);
			cylinder.radius = layer->Cylinder.CylinderRadius;
			cylinder.centralAngle = layer->Cylinder.CylinderAngle;
			cylinder.aspectRatio = layer->Cylinder.CylinderAspectRatio;
		}
		else if (type == ovrLayerType_Cube && Runtime::Get().CompositionCube)
		{
			if (!layer->Cube.CubeMapTexture)
				continue;

			XrCompositionLayerCubeKHR& cube = newLayer.Cube;
			cube = XR_TYPE(COMPOSITION_LAYER_CUBE_KHR);
			cube.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
			cube.swapchain = layer->Cube.CubeMapTexture->Swapchain;
			cube.imageArrayIndex = 0;
			cube.orientation = XR::Quatf(layer->Cube.Orientation);
		}
		else
		{
			// Layer type not recognized or disabled, ignore the layer
			assert(false);
			continue;
		}

		XrCompositionLayerBaseHeader& header = newLayer.Header;
		header.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		if (headLocked)
			header.space = session->ViewSpace;
		else
			header.space = session->TrackingSpaces[session->TrackingOrigin];

		layers.push_back(&newLayer.Header);
	}

	// If this frame index is in the past, find the correct frame based on the index
	XrIndexedFrameState* TargetFrame = session->CurrentFrame;
	if (frameIndex < TargetFrame->frameIndex)
		TargetFrame = &session->FrameStats[frameIndex % ovrMaxProvidedFrameStats];

	XrFrameEndInfo endInfo = XR_TYPE(FRAME_END_INFO);
	endInfo.displayTime = TargetFrame->predictedDisplayTime;
	endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endInfo.layerCount = (uint32_t)layers.size();
	endInfo.layers = layers.data();
	CHK_XR(xrEndFrame(session->Session, &endInfo));

	MicroProfileFlip();

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame2(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	REV_TRACE(ovr_SubmitFrame);
	MICROPROFILE_META_CPU("Submit Frame", (int)frameIndex);

	if (!session)
		return ovrError_InvalidSession;

	long long currentIndex = (*session->CurrentFrame).frameIndex;
	if (frameIndex <= 0)
		frameIndex = currentIndex;

	// Some older games submit frames redundantly, so we discard old frames in the legacy call
	if (frameIndex >= currentIndex)
		CHK_OVR(ovr_EndFrame(session, frameIndex, viewScaleDesc, layerPtrList, layerCount));
	CHK_OVR(ovr_WaitToBeginFrame(session, frameIndex + 1));
	CHK_OVR(ovr_BeginFrame(session, frameIndex + 1));
	return frameIndex < currentIndex ? ovrSuccess_NotVisible : ovrSuccess;
}

typedef struct OVR_ALIGNAS(4) ovrViewScaleDesc1_ {
	ovrVector3f HmdToEyeOffset[ovrEye_Count]; ///< Translation of each eye.
	float HmdSpaceToWorldScaleInMeters; ///< Ratio of viewer units to meter units.
} ovrViewScaleDesc1;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc1* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	// TODO: We don't ever use viewScaleDesc so no need to do any conversion.
	return ovr_SubmitFrame2(session, frameIndex, nullptr, layerPtrList, layerCount);
}

typedef struct OVR_ALIGNAS(4) ovrPerfStatsPerCompositorFrame1_
{
	int     HmdVsyncIndex;
	int     AppFrameIndex;
	int     AppDroppedFrameCount;
	float   AppMotionToPhotonLatency;
	float   AppQueueAheadTime;
	float   AppCpuElapsedTime;
	float   AppGpuElapsedTime;
	int     CompositorFrameIndex;
	int     CompositorDroppedFrameCount;
	float   CompositorLatency;
	float   CompositorCpuElapsedTime;
	float   CompositorGpuElapsedTime;
	float   CompositorCpuStartToGpuEndElapsedTime;
	float   CompositorGpuEndToVsyncElapsedTime;
} ovrPerfStatsPerCompositorFrame1;

typedef struct OVR_ALIGNAS(4) ovrPerfStats1_
{
	ovrPerfStatsPerCompositorFrame1  FrameStats[ovrMaxProvidedFrameStats];
	int                             FrameStatsCount;
	ovrBool                         AnyFrameStatsDropped;
	float                           AdaptiveGpuPerformanceScale;
} ovrPerfStats1;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetPerfStats(ovrSession session, ovrPerfStats* outStats)
{
	REV_TRACE(ovr_GetPerfStats);

	if (Runtime::Get().MinorVersion < 11)
		memset(outStats, 0, sizeof(ovrPerfStats1));
	else
		memset(outStats, 0, sizeof(ovrPerfStats));
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_ResetPerfStats(ovrSession session)
{
	REV_TRACE(ovr_ResetPerfStats);

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetPredictedDisplayTime(ovrSession session, long long frameIndex)
{
	if (!session)
		return ovr_GetTimeInSeconds();

	REV_TRACE(ovr_GetPredictedDisplayTime);
	XR_FUNCTION(session->Instance, ConvertTimeToWin32PerformanceCounterKHR);

	MICROPROFILE_META_CPU("Predict Frame", (int)frameIndex);

	XrIndexedFrameState* CurrentFrame = session->CurrentFrame;
	XrTime displayTime = CurrentFrame->predictedDisplayTime;
	if (frameIndex > CurrentFrame->frameIndex)
	{
		// There is no predicted display period for this frame yet, synthesize one
		displayTime += CurrentFrame->predictedDisplayPeriod * (frameIndex - CurrentFrame->frameIndex);
	}
	else if (frameIndex < CurrentFrame->frameIndex)
	{
		// We keep a history of older frames, check if this is within the history range
		// If not, then synthesize an older display time
		long long delta = CurrentFrame->frameIndex - frameIndex;
		if (delta < ovrMaxProvidedFrameStats)
			displayTime = session->FrameStats[frameIndex % ovrMaxProvidedFrameStats].predictedDisplayTime;
		else
			displayTime -= CurrentFrame->predictedDisplayPeriod * delta;
	}

	static double PerfFrequencyInverse = 0.0;
	if (PerfFrequencyInverse == 0.0)
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		PerfFrequencyInverse = 1.0 / (double)freq.QuadPart;
	}

	LARGE_INTEGER li;
	if (XR_FAILED(ConvertTimeToWin32PerformanceCounterKHR(session->Instance, displayTime, &li)))
		return 0.0;

	return li.QuadPart * PerfFrequencyInverse;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds()
{
	REV_TRACE(ovr_GetTimeInSeconds);

	static double PerfFrequencyInverse = 0.0;
	if (PerfFrequencyInverse == 0.0)
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		PerfFrequencyInverse = 1.0 / (double)freq.QuadPart;
	}

	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart * PerfFrequencyInverse;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_GetBool(ovrSession session, const char* propertyName, ovrBool defaultVal)
{
	REV_TRACE(ovr_GetBool);

	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetBool(ovrSession session, const char* propertyName, ovrBool value)
{
	REV_TRACE(ovr_SetBool);

	// TODO: Should we handle QueueAheadEnabled with always-on reprojection?
	return false;
}

OVR_PUBLIC_FUNCTION(int) ovr_GetInt(ovrSession session, const char* propertyName, int defaultVal)
{
	REV_TRACE(ovr_GetInt);

	if (strcmp("TextureSwapChainDepth", propertyName) == 0)
		return REV_DEFAULT_SWAPCHAIN_DEPTH;

	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetInt(ovrSession session, const char* propertyName, int value)
{
	REV_TRACE(ovr_SetInt);

	return false;
}

OVR_PUBLIC_FUNCTION(float) ovr_GetFloat(ovrSession session, const char* propertyName, float defaultVal)
{
	REV_TRACE(ovr_GetFloat);

	if (session)
	{
		if (strcmp(propertyName, "IPD") == 0)
			return XR::Vector3f(session->ViewPoses[ovrEye_Left].pose.position).Distance(
				XR::Vector3f(session->ViewPoses[ovrEye_Right].pose.position));

		if (strcmp(propertyName, "VsyncToNextVsync") == 0)
			return (*session->CurrentFrame).predictedDisplayPeriod / 1e9f;
	}

	// Override defaults, we should always return a valid value for these
	if (strcmp(propertyName, OVR_KEY_PLAYER_HEIGHT) == 0)
		defaultVal = OVR_DEFAULT_PLAYER_HEIGHT;
	else if (strcmp(propertyName, OVR_KEY_EYE_HEIGHT) == 0)
		defaultVal = OVR_DEFAULT_EYE_HEIGHT;

	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloat(ovrSession session, const char* propertyName, float value)
{
	REV_TRACE(ovr_SetFloat);

	return false;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetFloatArray(ovrSession session, const char* propertyName, float values[], unsigned int valuesCapacity)
{
	REV_TRACE(ovr_GetFloatArray);

	if (strcmp(propertyName, OVR_KEY_NECK_TO_EYE_DISTANCE) == 0)
	{
		if (valuesCapacity < 2)
			return 0;

		values[0] = OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL;
		values[1] = OVR_DEFAULT_NECK_TO_EYE_VERTICAL;
		return 2;
	}

	return 0;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloatArray(ovrSession session, const char* propertyName, const float values[], unsigned int valuesSize)
{
	REV_TRACE(ovr_SetFloatArray);

	return false;
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetString(ovrSession session, const char* propertyName, const char* defaultVal)
{
	REV_TRACE(ovr_GetString);

	if (!session)
		return defaultVal;

	// Override defaults, we should always return a valid value for these
	if (strcmp(propertyName, OVR_KEY_GENDER) == 0)
		defaultVal = OVR_DEFAULT_GENDER;

	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetString(ovrSession session, const char* propertyName, const char* value)
{
	REV_TRACE(ovr_SetString);

	return false;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Lookup(const char* name, void** data)
{
	// We don't communicate with the ovrServer.
	return ovrError_ServiceError;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetExternalCameras(ovrSession session, ovrExternalCamera* cameras, unsigned int* inoutCameraCount)
{
	// TODO: Support externalcamera.cfg used by the SteamVR Unity plugin
	return ovrError_NoExternalCameraInfo;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetExternalCameraProperties(ovrSession session, const char* name, const ovrCameraIntrinsics* const intrinsics, const ovrCameraExtrinsics* const extrinsics)
{
	return ovrError_NoExternalCameraInfo;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetEnabledCaps(ovrSession session)
{
	return 0;
}

OVR_PUBLIC_FUNCTION(void) ovr_SetEnabledCaps(ovrSession session, unsigned int hmdCaps)
{
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackingCaps(ovrSession session)
{
	return 0;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_ConfigureTracking(
	ovrSession session,
	unsigned int requestedTrackingCaps,
	unsigned int requiredTrackingCaps)
{
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_IsExtensionSupported(
	ovrSession session,
	ovrExtensions extension,
	ovrBool* outExtensionSupported)
{
	if (!outExtensionSupported)
		return ovrError_InvalidParameter;

	// TODO: Extensions support
	*outExtensionSupported = false;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_EnableExtension(ovrSession session, ovrExtensions extension)
{
	// TODO: Extensions support
	return ovrError_Unsupported;
}

typedef struct ovrViewportStencilDesc_ {
	ovrFovStencilType StencilType;
	ovrEyeType Eye;
	ovrFovPort FovPort; /// Typically Fov obtained from ovrEyeRenderDesc
	ovrQuatf HmdToEyeRotation; /// Typically HmdToEyePose.Orientation obtained from ovrEyeRenderDesc
} ovrViewportStencilDesc;

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetViewportStencil(
	ovrSession session,
	const ovrViewportStencilDesc* viewportStencilDesc,
	ovrFovStencilMeshBuffer* outMeshBuffer)
{
	ovrFovStencilDesc fovStencilDesc = {};
	fovStencilDesc.StencilType = viewportStencilDesc->StencilType;
	fovStencilDesc.StencilFlags = 0;
	fovStencilDesc.Eye = viewportStencilDesc->Eye;
	fovStencilDesc.FovPort = viewportStencilDesc->FovPort;
	fovStencilDesc.HmdToEyeRotation = viewportStencilDesc->HmdToEyeRotation;
	return ovr_GetFovStencil(session, &fovStencilDesc, outMeshBuffer);
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetFovStencil(
	ovrSession session,
	const ovrFovStencilDesc* fovStencilDesc,
	ovrFovStencilMeshBuffer* meshBuffer)
{
	if (!Runtime::Get().VisibilityMask)
		return ovrError_Unsupported;

	if (!session)
		return ovrError_InvalidSession;

	if (!meshBuffer || !fovStencilDesc)
		return ovrError_InvalidParameter;

	XrVisibilityMaskTypeKHR type = std::min((XrVisibilityMaskTypeKHR)(fovStencilDesc->StencilType + 1),
		XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR);
	const VisibilityMask& mask = session->VisibilityMasks[fovStencilDesc->Eye][type];

	// Some runtime advertise support for the extension, but don't always return valid masks
	if (mask.first.empty() || mask.second.empty())
	{
		std::vector<ovrVector2f> vertices;
		std::vector<uint16_t> indices;
		if (fovStencilDesc->StencilType == ovrFovStencil_HiddenArea)
		{
			vertices.push_back(OVR::Vector2f());
			indices.push_back(0);
			indices.push_back(0);
			indices.push_back(0);
		}
		else if (fovStencilDesc->StencilType == ovrFovStencil_VisibleArea)
		{
			vertices.push_back(OVR::Vector2f(0,0));
			vertices.push_back(OVR::Vector2f(1,0));
			vertices.push_back(OVR::Vector2f(1,1));
			vertices.push_back(OVR::Vector2f(0,1));
			indices.push_back(0);
			indices.push_back(1);
			indices.push_back(3);
			indices.push_back(1);
			indices.push_back(2);
			indices.push_back(3);
		}
		else if (fovStencilDesc->StencilType == ovrFovStencil_BorderLine || fovStencilDesc->StencilType == ovrFovStencil_VisibleRectangle)
		{
			vertices.push_back(OVR::Vector2f(0,0));
			vertices.push_back(OVR::Vector2f(1,0));
			vertices.push_back(OVR::Vector2f(1,1));
			vertices.push_back(OVR::Vector2f(0,1));
			indices.push_back(0);
			indices.push_back(1);
			indices.push_back(2);
			indices.push_back(3);
		}

		meshBuffer->UsedVertexCount = (int)vertices.size();
		meshBuffer->UsedIndexCount = (int)indices.size();
		if (!meshBuffer->AllocVertexCount && !meshBuffer->AllocIndexCount)
			return ovrSuccess;
		else if (meshBuffer->AllocVertexCount < meshBuffer->UsedVertexCount ||
			meshBuffer->AllocIndexCount < meshBuffer->UsedIndexCount ||
			!meshBuffer->VertexBuffer || !meshBuffer->IndexBuffer)
			return ovrError_InvalidParameter;

		memcpy(meshBuffer->VertexBuffer, vertices.data(), vertices.size() * sizeof(ovrVector2f));
		memcpy(meshBuffer->IndexBuffer, indices.data(), indices.size() * sizeof(uint16_t));
	}

	OVR::ScaleAndOffset2D scaleAndOffset = OVR::FovPort::CreateNDCScaleAndOffsetFromFov(
		XR::FovPort(session->ViewFov[fovStencilDesc->Eye].recommendedFov));

	// Visibility masks are in view space at z=-1, so we need to construct
	// a right-handed 2D projection matrix to project the mask to NDC space
	// TODO: Support the eye orientation in fovStencilDesc
	OVR::Matrix3f matrix(
		scaleAndOffset.Scale.x, 0.0f, -scaleAndOffset.Offset.x,
		0.0f, scaleAndOffset.Scale.y, -scaleAndOffset.Offset.y,
		0.0f, 0.0f, 0.0f); // We're not interested in the Z value

	if (Runtime::Get().UseHack(Runtime::HACK_NDC_MASKS))
		matrix = OVR::Matrix3f::Identity();

	const float scale = fovStencilDesc->StencilFlags & ovrFovStencilFlag_MeshOriginAtBottomLeft ? 2.0f : -2.0f;
	auto ToUVSpace = [matrix, scale](XR::Vector2f v)
	{
		// Transform the vertex at the correct distance from the eye
		OVR::Vector3f result = matrix.Transform(OVR::Vector3f(v.x, v.y, -1.0f));

		// Translate all NDC space vertices to UV space coordinate range [0,1]
		return OVR::Vector2f(result.x, result.y) / scale + OVR::Vector2f(0.5f);
	};

	// For the visible rectangle we use the line loop to find rectangle that fits in the visible area
	if (fovStencilDesc->StencilType == ovrFovStencil_VisibleRectangle)
	{
		meshBuffer->UsedVertexCount = 4;
		meshBuffer->UsedIndexCount = 6;
		if (!meshBuffer->AllocVertexCount && !meshBuffer->AllocIndexCount)
			return ovrSuccess;
		else if (meshBuffer->AllocVertexCount < meshBuffer->UsedVertexCount ||
			meshBuffer->AllocIndexCount < meshBuffer->UsedIndexCount ||
			!meshBuffer->VertexBuffer || !meshBuffer->IndexBuffer)
			return ovrError_InvalidParameter;

		// Find a line segment in each quadrant closest to diagonal, these will serve as the vertices of our rectangle
		ovrVector2f vertices[] = { { -1.0f, 1.0f }, { 1.0f, 1.0f },
			{ 1.0f, -1.0f },  { -1.0f, -1.0f } };
		float diagonality[] = { MATH_FLOAT_MAXVALUE, MATH_FLOAT_MAXVALUE,
			MATH_FLOAT_MAXVALUE,  MATH_FLOAT_MAXVALUE };
		for (size_t i = 0; i < mask.first.size(); i++)
		{
			// Compute the line segment and normalize it for comparison
			OVR::Vector2f line = XR::Vector2f(mask.first[(i + 1) % mask.first.size()]) - XR::Vector2f(mask.first[i]);
			OVR::Vector2f normalized = line.Normalized();

			// Find out the quadrant we're in based on a clockwise order starting from top-left
			uint8_t quadrant = abs((mask.first[i].y < 0) * 3 - (mask.first[i].x > 0));

			// Ignore line segments that are straight
			if (normalized.x == 0 || normalized.y == 0)
				continue;

			// Find out how close this segment is to being diagonal
			float delta = abs(abs(normalized.x) - abs(normalized.y));
			if (delta < diagonality[quadrant])
			{
				// Put the vertex half-way along the line segment
				vertices[quadrant] = XR::Vector2f(mask.first[i]) + line / 2.0f;
				diagonality[quadrant] = delta;
			}
		}

		// Align the rectangle to the axes
		vertices[0].x = vertices[3].x = std::max(vertices[0].x, vertices[3].x);
		vertices[0].y = vertices[1].y = std::min(vertices[0].y, vertices[1].y);
		vertices[1].x = vertices[2].x = std::min(vertices[1].x, vertices[2].x);
		vertices[2].y = vertices[3].y = std::max(vertices[2].y, vertices[3].y);

		// Translate all NDC space vertices to UV space coordinate range [0,1]
		for (int i = 0; i < meshBuffer->AllocVertexCount; i++)
			meshBuffer->VertexBuffer[i] = ToUVSpace(vertices[i]);

		uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };
		memcpy(meshBuffer->IndexBuffer, indices, sizeof(indices));
		return ovrSuccess;
	}

	meshBuffer->UsedVertexCount = (int)mask.first.size();
	meshBuffer->UsedIndexCount = (int)mask.second.size();
	if (!meshBuffer->AllocVertexCount && !meshBuffer->AllocIndexCount)
		return ovrSuccess;
	else if (meshBuffer->AllocVertexCount < meshBuffer->UsedVertexCount ||
			meshBuffer->AllocIndexCount < meshBuffer->UsedIndexCount ||
			!meshBuffer->VertexBuffer || !meshBuffer->IndexBuffer)
		return ovrError_InvalidParameter;

	for (int i = 0; i < meshBuffer->UsedVertexCount; i++)
		meshBuffer->VertexBuffer[i] = ToUVSpace(mask.first[i]);

	for (int i = 0; i < meshBuffer->UsedIndexCount; i++)
		meshBuffer->IndexBuffer[i] = (uint16_t)mask.second[i];

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrHmdColorDesc)
ovr_GetHmdColorDesc(ovrSession session)
{
	ovrHmdColorDesc desc = { ovrColorSpace_Unknown };
	if (session && Runtime::Get().ColorSpace)
	{
		switch (session->SystemColorSpace.colorSpace)
		{
		case XR_COLOR_SPACE_REC2020_FB:
			desc.ColorSpace = ovrColorSpace_Rec_2020;
			break;
		case XR_COLOR_SPACE_REC709_FB:
			desc.ColorSpace = ovrColorSpace_Rec_709;
			break;
		case XR_COLOR_SPACE_RIFT_CV1_FB:
			desc.ColorSpace = ovrColorSpace_Rift_CV1;
			break;
		case XR_COLOR_SPACE_RIFT_S_FB:
			desc.ColorSpace = ovrColorSpace_Rift_S;
			break;
		case XR_COLOR_SPACE_QUEST_FB:
			desc.ColorSpace = ovrColorSpace_Quest;
			break;
		default:
			desc.ColorSpace = ovrColorSpace(session->SystemColorSpace.colorSpace + 1);
			break;
		}
	}
	return desc;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_SetClientColorDesc(ovrSession session, const ovrHmdColorDesc* colorDesc)
{
	if (!Runtime::Get().ColorSpace)
		return ovrError_Unsupported;

	if (!session)
		return ovrError_InvalidSession;

	XR_FUNCTION(session->Instance, SetColorSpaceFB);

	XrColorSpaceFB colorSpace = XR_COLOR_SPACE_UNMANAGED_FB;
	switch (colorDesc->ColorSpace)
	{
	case ovrColorSpace_Rec_2020:
		colorSpace = XR_COLOR_SPACE_REC2020_FB;
		break;
	case ovrColorSpace_Rec_709:
		colorSpace = XR_COLOR_SPACE_REC709_FB;
		break;
	case ovrColorSpace_Unknown:
	case ovrColorSpace_Rift_CV1:
		colorSpace = XR_COLOR_SPACE_RIFT_CV1_FB;
		break;
	case ovrColorSpace_Rift_S:
		colorSpace = XR_COLOR_SPACE_RIFT_S_FB;
		break;
	case ovrColorSpace_Quest:
		colorSpace = XR_COLOR_SPACE_QUEST_FB;
		break;
	default:
		colorSpace = XrColorSpaceFB(colorDesc->ColorSpace - 1);
		break;
	}

	CHK_XR(SetColorSpaceFB(session->Session, colorSpace));
	return ovrSuccess;
}

struct ovrDesktopWindowDesc_;
typedef struct ovrDesktopWindowDesc_ ovrDesktopWindowDesc;

struct ovrHybridInputFocusState_;
typedef struct ovrHybridInputFocusState_ ovrHybridInputFocusState;

typedef uint32_t ovrDesktopWindowHandle;

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_InitDesktopWindow(
	ovrSession session,
	ovrDesktopWindowHandle* outWindowHandle)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_ShowDesktopWindow(
	ovrSession session,
	const ovrDesktopWindowDesc* windowDesc)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_HideDesktopWindow(
	ovrSession session,
	ovrDesktopWindowHandle windowHandle)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetHybridInputFocus(
	ovrSession session,
	ovrControllerType controllerType,
	ovrHybridInputFocusState* outState)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_ShowAvatarHands(
	ovrSession session,
	ovrBool showHands)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_ShowKeyboard()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_EnableHybridRaycast()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_QueryDistortion()
{
	return ovrError_Unsupported;
}
