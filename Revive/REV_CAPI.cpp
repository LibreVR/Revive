#include "OVR_CAPI.h"
#include "OVR_Version.h"
#include "REV_Math.h"

#include "Assert.h"
#include "Session.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"
#include "Settings.h"
#include "SettingsManager.h"

#include <dxgi1_2.h>
#include <openvr.h>
#include <MinHook.h>
#include <list>
#include <algorithm>

#define REV_DEFAULT_TIMEOUT 10000

vr::EVRInitError g_InitError = vr::VRInitError_Init_NotInitialized;
uint32_t g_MinorVersion = OVR_MINOR_VERSION;
std::list<ovrHmdStruct> g_Sessions;

ovrResult rev_InitErrorToOvrError(vr::EVRInitError error)
{
	switch (error)
	{
	case vr::VRInitError_None: return ovrSuccess;
	case vr::VRInitError_Unknown: return ovrError_Initialize;
	case vr::VRInitError_Init_InstallationNotFound: return ovrError_LibLoad;
	case vr::VRInitError_Init_InstallationCorrupt: return ovrError_LibLoad;
	case vr::VRInitError_Init_VRClientDLLNotFound: return ovrError_LibLoad;
	case vr::VRInitError_Init_FileNotFound: return ovrError_LibLoad;
	case vr::VRInitError_Init_FactoryNotFound: return ovrError_ServiceConnection;
	case vr::VRInitError_Init_InterfaceNotFound: return ovrError_ServiceConnection;
	case vr::VRInitError_Init_InvalidInterface: return ovrError_MismatchedAdapters;
	case vr::VRInitError_Init_UserConfigDirectoryInvalid: return ovrError_Initialize;
	case vr::VRInitError_Init_HmdNotFound: return ovrError_NoHmd;
	case vr::VRInitError_Init_NotInitialized: return ovrError_Initialize;
	case vr::VRInitError_Init_PathRegistryNotFound: return ovrError_Initialize;
	case vr::VRInitError_Init_NoConfigPath: return ovrError_Initialize;
	case vr::VRInitError_Init_NoLogPath: return ovrError_Initialize;
	case vr::VRInitError_Init_PathRegistryNotWritable: return ovrError_Initialize;
	case vr::VRInitError_Init_AppInfoInitFailed: return ovrError_ServerStart;
		//case vr::VRInitError_Init_Retry: return ovrError_Reinitialization; // Internal
	case vr::VRInitError_Init_InitCanceledByUser: return ovrError_Initialize;
	case vr::VRInitError_Init_AnotherAppLaunching: return ovrError_ServerStart;
	case vr::VRInitError_Init_SettingsInitFailed: return ovrError_Initialize;
	case vr::VRInitError_Init_ShuttingDown: return ovrError_ServerStart;
	case vr::VRInitError_Init_TooManyObjects: return ovrError_Initialize;
	case vr::VRInitError_Init_NoServerForBackgroundApp: return ovrError_ServerStart;
	case vr::VRInitError_Init_NotSupportedWithCompositor: return ovrError_Initialize;
	case vr::VRInitError_Init_NotAvailableToUtilityApps: return ovrError_Initialize;
	default: return ovrError_RuntimeException;
	}
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params)
{
	MicroProfileOnThreadCreate("Main");
	MicroProfileSetForceEnable(true);
	MicroProfileSetEnableAllGroups(true);
	MicroProfileSetForceMetaCounters(true);
	MicroProfileWebServerStart();

	g_MinorVersion = params->RequestedMinorVersion;

	MH_QueueDisableHook(LoadLibraryW);
	MH_QueueDisableHook(OpenEventW);
	MH_ApplyQueued();

	vr::VR_Init(&g_InitError, vr::VRApplication_Scene);

	MH_QueueEnableHook(LoadLibraryW);
	MH_QueueEnableHook(OpenEventW);
	MH_ApplyQueued();

	uint32_t timeout = params->ConnectionTimeoutMS;
	if (timeout == 0)
		timeout = REV_DEFAULT_TIMEOUT;

	// Wait until the compositor is ready, if SteamVR still needs to start the nominal waiting time
	// is 1.5 seconds. Thus we don't even attempt to wait if the requested timeout is only 100ms.
	while (timeout > 100 && vr::VRCompositor() == nullptr)
	{
		Sleep(100);
		timeout -= 100;
	}

	if (vr::VRCompositor() == nullptr)
		return ovrError_Timeout;

	return rev_InitErrorToOvrError(g_InitError);
}

OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{
	g_Sessions.clear();
	vr::VR_Shutdown();
	MicroProfileShutdown();
}

OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo)
{
	REV_TRACE(ovr_GetLastErrorInfo);

	if (!errorInfo)
		return;

	const char* error = VR_GetVRInitErrorAsEnglishDescription(g_InitError);
	strncpy_s(errorInfo->ErrorString, error, sizeof(ovrErrorInfo::ErrorString));
	errorInfo->Result = rev_InitErrorToOvrError(g_InitError);
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

	if (!session)
	{
		ovrHmdDesc desc = {};
		desc.Type = vr::VR_IsHmdPresent() ? ovrHmd_CV1 : ovrHmd_None;
		return desc;
	}

	return *session->Details->HmdDesc;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackerCount(ovrSession session)
{
	REV_TRACE(ovr_GetTrackerCount);

	if (!session)
		return ovrError_InvalidSession;

	if (session->Details->UseHack(SessionDetails::HACK_SPOOF_SENSORS))
		return 2;

	return (unsigned int)session->Details->TrackerCount;
}

OVR_PUBLIC_FUNCTION(ovrTrackerDesc) ovr_GetTrackerDesc(ovrSession session, unsigned int trackerDescIndex)
{
	REV_TRACE(ovr_GetTrackerDesc);

	if (!session)
		return ovrTrackerDesc();

	ovrTrackerDesc* pDesc = session->Details->TrackerDesc[trackerDescIndex];

	if (!pDesc)
		return ovrTrackerDesc();

	return *pDesc;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Create(ovrSession* pSession, ovrGraphicsLuid* pLuid)
{
	REV_TRACE(ovr_Create);

	if (!pSession)
		return ovrError_InvalidParameter;

	*pSession = nullptr;

	// Initialize the opaque pointer with our own OpenVR-specific struct
	g_Sessions.emplace_back();

	// Get the LUID for the OpenVR adapter
	uint64_t adapter;
	vr::VRSystem()->GetOutputDevice(&adapter, vr::TextureType_DirectX);
	if (adapter)
	{
		// Copy the LUID into the structure
		static_assert(sizeof(adapter) == sizeof(ovrGraphicsLuid),
			"The adapter LUID needs to fit in ovrGraphicsLuid");
		if (pLuid)
			memcpy(pLuid, &adapter, sizeof(ovrGraphicsLuid));
	}
	else
	{
		// Fall-back to the default adapter
		IDXGIFactory1* pFactory = nullptr;
		if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory)))
			return ovrError_RuntimeException;

		IDXGIAdapter1* pAdapter = nullptr;
		if (FAILED(pFactory->EnumAdapters1(0, &pAdapter)))
			return ovrError_RuntimeException;

		DXGI_ADAPTER_DESC1 desc;
		if (FAILED(pAdapter->GetDesc1(&desc)))
			return ovrError_RuntimeException;

		// Copy the LUID into the structure
		static_assert(sizeof(desc.AdapterLuid) == sizeof(ovrGraphicsLuid),
			"The adapter LUID needs to fit in ovrGraphicsLuid");
		if (pLuid)
			memcpy(pLuid, &desc.AdapterLuid, sizeof(ovrGraphicsLuid));
	}

	*pSession = &g_Sessions.back();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_Destroy(ovrSession session)
{
	REV_TRACE(ovr_Destroy);

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

	// Detect if the application has focus, but only return false the first time the status is requested.
	// If this is true from the first call then Airmech will assume the Health-and-Safety warning
	// is still being displayed.
	static bool firstCall = true;
	sessionStatus->IsVisible = vr::VRCompositor()->CanRenderScene() && !firstCall;
	firstCall = false;

	SessionStatusBits status = session->SessionStatus;

	// Don't use the activity level while debugging, so I don't have to put on the HMD
	sessionStatus->HmdPresent = status.HmdPresent;
	sessionStatus->HmdMounted = status.HmdMounted;

	// TODO: Detect if the display is lost, can this ever happen with OpenVR?
	sessionStatus->DisplayLost = status.DisplayLost;
	sessionStatus->ShouldQuit = status.ShouldQuit;
	sessionStatus->ShouldRecenter = status.ShouldRecenter;
	sessionStatus->HasInputFocus = status.HasInputFocus;
	sessionStatus->OverlayPresent = status.OverlayPresent;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetTrackingOriginType(ovrSession session, ovrTrackingOrigin origin)
{
	REV_TRACE(ovr_SetTrackingOriginType);

	if (!session)
		return ovrError_InvalidSession;

	// Both enums match exactly, so we can just cast them
	session->TrackingOrigin = (vr::ETrackingUniverseOrigin)origin;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrTrackingOrigin) ovr_GetTrackingOriginType(ovrSession session)
{
	REV_TRACE(ovr_GetTrackingOriginType);

	if (!session)
		return ovrTrackingOrigin_EyeLevel;

	// Both enums match exactly, so we can just cast them
	return (ovrTrackingOrigin)session->TrackingOrigin;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RecenterTrackingOrigin(ovrSession session)
{
	REV_TRACE(ovr_RecenterTrackingOrigin);

	if (!session)
		return ovrError_InvalidSession;

	vr::VRSystem()->ResetSeatedZeroPose();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SpecifyTrackingOrigin(ovrSession session, ovrPosef originPose)
{
	// TODO: Implement through ApplyTransform()
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_ClearShouldRecenterFlag(ovrSession session) { /* No such flag, do nothing */ }

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingState(ovrSession session, double absTime, ovrBool latencyMarker)
{
	REV_TRACE(ovr_GetTrackingState);

	ovrTrackingState state = { 0 };

	if (!session)
		return state;

	session->Input->GetTrackingState(session, &state, absTime);
	return state;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetDevicePoses(ovrSession session, ovrTrackedDeviceType* deviceTypes, int deviceCount, double absTime, ovrPoseStatef* outDevicePoses)
{
	REV_TRACE(ovr_GetDevicePoses);

	if (!session)
		return ovrError_InvalidSession;

	return session->Input->GetDevicePoses(deviceTypes, deviceCount, absTime, outDevicePoses);
}

struct ovrSensorData_;
typedef struct ovrSensorData_ ovrSensorData;

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingStateWithSensorData(ovrSession session, double absTime, ovrBool latencyMarker, ovrSensorData* sensorData)
{
	REV_TRACE(ovr_GetTrackingStateWithSensorData);

	// This is a private API, ignore the raw sensor data request and hope for the best.
	REV_ASSERT(sensorData == nullptr);

	return ovr_GetTrackingState(session, absTime, latencyMarker);
}

OVR_PUBLIC_FUNCTION(ovrTrackerPose) ovr_GetTrackerPose(ovrSession session, unsigned int trackerPoseIndex)
{
	REV_TRACE(ovr_GetTrackerPose);

	ovrTrackerPose tracker = { 0 };

	if (!session)
		return tracker;

	if (session->Details->UseHack(SessionDetails::HACK_SPOOF_SENSORS))
	{
		tracker.LeveledPose = OVR::Posef::Identity();
		tracker.Pose = OVR::Posef::Identity();
		tracker.TrackerFlags = ovrTracker_Connected;
		return tracker;
	}

	// Get the index for this tracker.
	vr::TrackedDeviceIndex_t trackers[vr::k_unMaxTrackedDeviceCount] = { vr::k_unTrackedDeviceIndexInvalid };
	vr::VRSystem()->GetSortedTrackedDeviceIndicesOfClass(vr::TrackedDeviceClass_TrackingReference, trackers, vr::k_unMaxTrackedDeviceCount);
	vr::TrackedDeviceIndex_t index = trackers[trackerPoseIndex];

	// Get the device poses.
	vr::TrackedDevicePose_t pose;
	vr::VRCompositor()->GetLastPoseForTrackedDeviceIndex(index, &pose, nullptr);

	// TODO: Should the tracker pose always be in the standing universe?
	//vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0, poses, vr::k_unMaxTrackedDeviceCount);

	// Set the flags
	tracker.TrackerFlags = 0;
	if (index != vr::k_unTrackedDeviceIndexInvalid)
	{
		if (vr::VRSystem()->IsTrackedDeviceConnected(index))
			tracker.TrackerFlags |= ovrTracker_Connected;
		if (pose.bPoseIsValid)
			tracker.TrackerFlags |= ovrTracker_PoseTracked;
	}

	// Convert the pose
	REV::Matrix4f matrix;
	if (index != vr::k_unTrackedDeviceIndexInvalid && pose.bPoseIsValid)
		matrix = REV::Matrix4f(pose.mDeviceToAbsoluteTracking);

	// We need to mirror the orientation along either the X or Y axis
	OVR::Quatf quat = OVR::Quatf(matrix);
	OVR::Quatf mirror = OVR::Quatf(1.0f, 0.0f, 0.0f, 0.0f);
	tracker.Pose.Orientation = quat * mirror;
	tracker.Pose.Position = matrix.GetTranslation();

	// Level the pose
	float yaw;
	quat.GetYawPitchRoll(&yaw, nullptr, nullptr);
	tracker.LeveledPose.Orientation = OVR::Quatf(OVR::Axis_Y, yaw);
	tracker.LeveledPose.Position = matrix.GetTranslation();

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

	if (!session)
		return ovrError_InvalidSession;

	if (!inputState)
		return ovrError_InvalidParameter;

	ovrInputState state = { 0 };
	ovrResult result = session->Input->GetInputState(session, controllerType, &state);

	// We need to make sure we don't write outside of the bounds of the struct
	// when the client expects a pre-1.7 version of LibOVR.
	if (g_MinorVersion < 7)
		memcpy(inputState, &state, sizeof(ovrInputState1));
	else if (g_MinorVersion < 11)
		memcpy(inputState, &state, sizeof(ovrInputState2));
	else
		memcpy(inputState, &state, sizeof(ovrInputState));

	return result;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetConnectedControllerTypes(ovrSession session)
{
	REV_TRACE(ovr_GetConnectedControllerTypes);

	return session->Input->ConnectedControllers;
}

OVR_PUBLIC_FUNCTION(ovrTouchHapticsDesc) ovr_GetTouchHapticsDesc(ovrSession session, ovrControllerType controllerType)
{
	REV_TRACE(ovr_GetTouchHapticsDesc);

	return session->Input->GetTouchHapticsDesc(controllerType);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	REV_TRACE(ovr_SetControllerVibration);

	if (!session)
		return ovrError_InvalidSession;

	return session->Input->SetControllerVibration(session, controllerType, frequency, amplitude);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitControllerVibration(ovrSession session, ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	REV_TRACE(ovr_SubmitControllerVibration);

	if (!session)
		return ovrError_InvalidSession;

	return session->Input->SubmitControllerVibration(session, controllerType, buffer);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState)
{
	REV_TRACE(ovr_GetControllerVibrationState);

	if (!session)
		return ovrError_InvalidSession;

	return session->Input->GetControllerVibrationState(session, controllerType, outState);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_TestBoundary(ovrSession session, ovrTrackedDeviceType deviceBitmask,
	ovrBoundaryType boundaryType, ovrBoundaryTestResult* outTestResult)
{
	REV_TRACE(ovr_TestBoundary);

	outTestResult->ClosestDistance = INFINITY;

	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	vr::VRCompositor()->GetLastPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

	if (vr::VRChaperone()->GetCalibrationState() != vr::ChaperoneCalibrationState_OK)
		return ovrSuccess_BoundaryInvalid;

	if (deviceBitmask & ovrTrackedDevice_HMD)
	{
		ovrBoundaryTestResult result = { 0 };
		REV::Matrix4f matrix = (REV::Matrix4f)poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
		ovrVector3f point = matrix.GetTranslation();

		ovrResult err = ovr_TestBoundaryPoint(session, &point, boundaryType, &result);
		if (OVR_SUCCESS(err) && result.ClosestDistance < outTestResult->ClosestDistance)
			*outTestResult = result;
	}


	vr::TrackedDeviceIndex_t hands[] = { vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),
		vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) };

	for (int i = 0; i < ovrHand_Count; i++)
	{
		if (deviceBitmask & (ovrTrackedDevice_LTouch << i))
		{
			ovrBoundaryTestResult result = { 0 };
			if (hands[i] != vr::k_unTrackedDeviceIndexInvalid)
			{
				REV::Matrix4f matrix = (REV::Matrix4f)poses[hands[i]].mDeviceToAbsoluteTracking;
				ovrVector3f point = matrix.GetTranslation();

				ovrResult err = ovr_TestBoundaryPoint(session, &point, boundaryType, &result);
				if (OVR_SUCCESS(err) && result.ClosestDistance < outTestResult->ClosestDistance)
					*outTestResult = result;
			}
		}
	}

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_TestBoundaryPoint(ovrSession session, const ovrVector3f* point,
	ovrBoundaryType singleBoundaryType, ovrBoundaryTestResult* outTestResult)
{
	REV_TRACE(ovr_TestBoundaryPoint);

	ovrBoundaryTestResult result = { 0 };

	if (vr::VRChaperone()->GetCalibrationState() != vr::ChaperoneCalibrationState_OK)
		return ovrSuccess_BoundaryInvalid;

	result.IsTriggering = vr::VRChaperone()->AreBoundsVisible();

	OVR::Vector2f playArea;
	if (!vr::VRChaperone()->GetPlayAreaSize(&playArea.x, &playArea.y))
		return ovrSuccess_BoundaryInvalid;

	// Clamp the point to the AABB
	OVR::Vector2f p(point->x, point->z);
	OVR::Vector2f halfExtents(playArea.x / 2.0f, playArea.y / 2.0f);
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

	vr::HmdColor_t color = (vr::HmdColor_t&)lookAndFeel->Color;
	vr::VRChaperone()->SetSceneColor(color);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_ResetBoundaryLookAndFeel(ovrSession session)
{
	REV_TRACE(ovr_ResetBoundaryLookAndFeel);

	vr::HmdColor_t color = { 1.0f, 1.0f, 1.0f, 1.0f };
	vr::VRChaperone()->SetSceneColor(color);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryGeometry(ovrSession session, ovrBoundaryType boundaryType, ovrVector3f* outFloorPoints, int* outFloorPointsCount)
{
	REV_TRACE(ovr_GetBoundaryGeometry);

	vr::HmdQuad_t playRect;
	bool valid = vr::VRChaperone()->GetPlayAreaRect(&playRect);
	if (outFloorPoints)
		memcpy(outFloorPoints, playRect.vCorners, 4 * sizeof(ovrVector3f));
	if (outFloorPointsCount)
		*outFloorPointsCount = valid ? 4 : 0;
	return valid ? ovrSuccess : ovrSuccess_BoundaryInvalid;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryDimensions(ovrSession session, ovrBoundaryType boundaryType, ovrVector3f* outDimensions)
{
	REV_TRACE(ovr_GetBoundaryDimensions);

	outDimensions->y = 0.0f; // TODO: Find some good default height
	bool valid = vr::VRChaperone()->GetPlayAreaSize(&outDimensions->x, &outDimensions->z);
	return valid ? ovrSuccess : ovrSuccess_BoundaryInvalid;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryVisible(ovrSession session, ovrBool* outIsVisible)
{
	REV_TRACE(ovr_GetBoundaryVisible);

	*outIsVisible = vr::VRChaperone()->AreBoundsVisible();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RequestBoundaryVisible(ovrSession session, ovrBool visible)
{
	REV_TRACE(ovr_RequestBoundaryVisible);

	vr::VRChaperone()->ForceBoundsVisible(!!visible);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainLength(ovrSession session, ovrTextureSwapChain chain, int* out_Length)
{
	REV_TRACE(ovr_GetTextureSwapChainLength);

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", chain->Identifier);
	*out_Length = chain->Length;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainCurrentIndex(ovrSession session, ovrTextureSwapChain chain, int* out_Index)
{
	REV_TRACE(ovr_GetTextureSwapChainCurrentIndex);

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", chain->Identifier);
	MICROPROFILE_META_CPU("Index", chain->CurrentIndex);
	*out_Index = chain->CurrentIndex;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainDesc(ovrSession session, ovrTextureSwapChain chain, ovrTextureSwapChainDesc* out_Desc)
{
	REV_TRACE(ovr_GetTextureSwapChainDesc);

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", chain->Identifier);
	*out_Desc = chain->Desc;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CommitTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	REV_TRACE(ovr_CommitTextureSwapChain);

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", chain->Identifier);
	MICROPROFILE_META_CPU("CurrentIndex", chain->CurrentIndex);
	MICROPROFILE_META_CPU("SubmitIndex", chain->SubmitIndex);

	if (chain->Full())
		return ovrError_TextureSwapChainFull;

	chain->Commit();

	if (chain->Overlay != vr::k_ulOverlayHandleInvalid)
	{
		vr::VRTextureWithPose_t texture = chain->Textures[chain->SubmitIndex]->ToVRTexture();
		vr::VROverlay()->SetOverlayTexture(chain->Overlay, &texture);
	}

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	REV_TRACE(ovr_DestroyTextureSwapChain);

	if (!chain)
		return;

	MICROPROFILE_META_CPU("Identifier", chain->Identifier);
	vr::VROverlay()->DestroyOverlay(chain->Overlay);
	delete chain;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyMirrorTexture(ovrSession session, ovrMirrorTexture mirrorTexture)
{
	REV_TRACE(ovr_DestroyMirrorTexture);

	if (!mirrorTexture)
		return;

	session->Compositor->SetMirrorTexture(nullptr);
	delete mirrorTexture;
}

OVR_PUBLIC_FUNCTION(ovrSizei) ovr_GetFovTextureSize(ovrSession session, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel)
{
	REV_TRACE(ovr_GetFovTextureSize);

	// Get the descriptor for this eye
	ovrEyeRenderDesc* desc = session->Details->RenderDesc[eye];
	ovrSizei size = desc->DistortedViewport.Size;

	// Grow the recommended size to account for the overlapping fov
	// TODO: Add a setting to ignore pixelsPerDisplayPixel
	vr::VRTextureBounds_t bounds = CompositorBase::FovPortToTextureBounds(desc->Fov, fov);
	size.w = int((size.w * pixelsPerDisplayPixel) / (bounds.uMax - bounds.uMin));
	size.h = int((size.h * pixelsPerDisplayPixel) / (bounds.vMax - bounds.vMin));

	return size;
}

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovr_GetRenderDesc2(ovrSession session, ovrEyeType eyeType, ovrFovPort fov)
{
	REV_TRACE(ovr_GetRenderDesc);

	// Make a copy so we can adjust a few parameters
	ovrEyeRenderDesc desc = *session->Details->RenderDesc[eyeType];

	// Adjust the descriptor for the supplied field-of-view
	desc.Fov = fov;
	float WidthTan = fov.LeftTan + fov.RightTan;
	float HeightTan = fov.UpTan + fov.DownTan;
	desc.PixelsPerTanAngleAtCenter = OVR::Vector2f(desc.DistortedViewport.Size.w / WidthTan, desc.DistortedViewport.Size.h / HeightTan);

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

	if (!session || !session->Compositor)
		return ovrError_InvalidSession;

	return session->Compositor->WaitToBeginFrame(session, frameIndex);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_BeginFrame(ovrSession session, long long frameIndex)
{
	REV_TRACE(ovr_BeginFrame);
	MICROPROFILE_META_CPU("Begin Frame", (int)frameIndex);

	if (!session || !session->Compositor)
		return ovrError_InvalidSession;

	return session->Compositor->BeginFrame(session, frameIndex);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_EndFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	REV_TRACE(ovr_EndFrame);
	MICROPROFILE_META_CPU("End Frame", (int)frameIndex);

	if (!session || !session->Compositor)
		return ovrError_InvalidSession;

	// Use our own intermediate compositor to convert the frame to OpenVR.
	return session->Compositor->EndFrame(session, layerPtrList, layerCount);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame2(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	REV_TRACE(ovr_SubmitFrame);
	MICROPROFILE_META_CPU("Submit Frame", (int)frameIndex);

	if (!session || !session->Compositor)
		return ovrError_InvalidSession;

	if (frameIndex == 0)
		frameIndex = session->FrameIndex;

	// Use our own intermediate compositor to convert the frame to OpenVR.
	ovrResult result = session->Compositor->EndFrame(session, layerPtrList, layerCount);

	// Begin the next frame
	if (!session->Details->UseHack(SessionDetails::HACK_WAIT_IN_TRACKING_STATE))
		session->Compositor->WaitToBeginFrame(session, frameIndex + 1);
	session->Compositor->BeginFrame(session, frameIndex + 1);

	return result;
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

	ovrPerfStatsPerCompositorFrame FrameStats[ovrMaxProvidedFrameStats];

	// TODO: Implement performance scale heuristics
	float AdaptiveGpuPerformanceScale = 1.0f;
	bool AnyFrameStatsDropped = (session->FrameIndex - session->StatsIndex) > ovrMaxProvidedFrameStats;
	int FrameStatsCount = AnyFrameStatsDropped ? ovrMaxProvidedFrameStats : int(session->FrameIndex - session->StatsIndex);
	session->StatsIndex = session->FrameIndex;

	vr::Compositor_FrameTiming TimingStats[ovrMaxProvidedFrameStats];
	TimingStats[0].m_nSize = sizeof(vr::Compositor_FrameTiming);
	vr::VRCompositor()->GetFrameTimings(TimingStats, FrameStatsCount);

	vr::Compositor_CumulativeStats TotalStats;
	vr::VRCompositor()->GetCumulativeStats(&TotalStats, sizeof(vr::Compositor_CumulativeStats));

	// Subtract the base stats so we get the cumulative stats since the last call to ovr_ResetPerfStats
	TotalStats.m_nNumFramePresents -= session->BaseStats.m_nNumFramePresents;
	TotalStats.m_nNumDroppedFrames -= session->BaseStats.m_nNumDroppedFrames;
	TotalStats.m_nNumReprojectedFrames -= session->BaseStats.m_nNumReprojectedFrames;
	TotalStats.m_nNumFramePresentsOnStartup -= session->BaseStats.m_nNumFramePresentsOnStartup;
	TotalStats.m_nNumDroppedFramesOnStartup -= session->BaseStats.m_nNumDroppedFramesOnStartup;
	TotalStats.m_nNumReprojectedFramesOnStartup -= session->BaseStats.m_nNumReprojectedFramesOnStartup;
	TotalStats.m_nNumLoading -= session->BaseStats.m_nNumLoading;
	TotalStats.m_nNumFramePresentsLoading -= session->BaseStats.m_nNumFramePresentsLoading;
	TotalStats.m_nNumDroppedFramesLoading -= session->BaseStats.m_nNumDroppedFramesLoading;
	TotalStats.m_nNumReprojectedFramesLoading -= session->BaseStats.m_nNumReprojectedFramesLoading;
	TotalStats.m_nNumTimedOut -= session->BaseStats.m_nNumTimedOut;
	TotalStats.m_nNumFramePresentsTimedOut -= session->BaseStats.m_nNumFramePresentsTimedOut;
	TotalStats.m_nNumDroppedFramesTimedOut -= session->BaseStats.m_nNumDroppedFramesTimedOut;
	TotalStats.m_nNumReprojectedFramesTimedOut -= session->BaseStats.m_nNumReprojectedFramesTimedOut;

	float fVsyncToPhotons = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);
	float fDisplayFrequency = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
	float fFrameDuration = 1.0f / fDisplayFrequency;
	for (int i = 0; i < FrameStatsCount; i++)
	{
		ovrPerfStatsPerCompositorFrame& stats = FrameStats[i];

		stats.HmdVsyncIndex = TotalStats.m_nNumFramePresents;
		stats.AppFrameIndex = (int)session->FrameIndex;
		stats.AppDroppedFrameCount = TotalStats.m_nNumDroppedFrames;
		// TODO: Improve latency handling with sensor timestamps and latency markers
		stats.AppMotionToPhotonLatency = fFrameDuration + fVsyncToPhotons;
		stats.AppQueueAheadTime = -TimingStats[i].m_flNewPosesReadyMs / 1000.0f;
		stats.AppCpuElapsedTime = TimingStats[i].m_flClientFrameIntervalMs / 1000.0f;
		stats.AppGpuElapsedTime = TimingStats[i].m_flPreSubmitGpuMs / 1000.0f;

		stats.CompositorFrameIndex = TimingStats[i].m_nFrameIndex;
		stats.CompositorDroppedFrameCount = (TotalStats.m_nNumDroppedFrames + TotalStats.m_nNumDroppedFramesLoading);
		stats.CompositorLatency = fVsyncToPhotons; // OpenVR doesn't have timewarp
		stats.CompositorCpuElapsedTime = TimingStats[i].m_flCompositorRenderCpuMs / 1000.0f;
		stats.CompositorGpuElapsedTime = TimingStats[i].m_flCompositorRenderGpuMs / 1000.0f;
		stats.CompositorCpuStartToGpuEndElapsedTime = (TimingStats[i].m_flCompositorUpdateEndMs - TimingStats[i].m_flCompositorRenderStartMs) / 1000.0f;
		stats.CompositorGpuEndToVsyncElapsedTime = TimingStats[i].m_flCompositorIdleCpuMs / 1000.0f;

		// TODO: Asynchronous Spacewap is not supported in OpenVR
		stats.AswIsActive = ovrFalse;
		stats.AswActivatedToggleCount = 0;
		stats.AswPresentedFrameCount = 0;
		stats.AswFailedFrameCount = 0;
	}

	// We need to make sure we don't write outside of the bounds of the struct in older version of the runtime
	if (g_MinorVersion < 11)
	{
		ovrPerfStats1* out = (ovrPerfStats1*)outStats;
		for (int i = 0; i < FrameStatsCount; i++)
			memcpy(out->FrameStats + i, FrameStats + i, sizeof(ovrPerfStatsPerCompositorFrame1));
		out->AdaptiveGpuPerformanceScale = AdaptiveGpuPerformanceScale;
		out->AnyFrameStatsDropped = AnyFrameStatsDropped;
		out->FrameStatsCount = FrameStatsCount;
	}
	else
	{
		ovrPerfStats* out = outStats;
		memcpy(out->FrameStats, FrameStats, sizeof(FrameStats));
		out->AdaptiveGpuPerformanceScale = AdaptiveGpuPerformanceScale;
		out->AnyFrameStatsDropped = AnyFrameStatsDropped;
		out->FrameStatsCount = FrameStatsCount;
		out->AswIsAvailable = ovrFalse;

		if (g_MinorVersion >= 14)
			out->VisibleProcessId = vr::VRCompositor()->GetCurrentSceneFocusProcess();
	}

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_ResetPerfStats(ovrSession session)
{
	REV_TRACE(ovr_ResetPerfStats);

	vr::VRCompositor()->GetCumulativeStats(&session->BaseStats, sizeof(vr::Compositor_CumulativeStats));
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetPredictedDisplayTime(ovrSession session, long long frameIndex)
{
	REV_TRACE(ovr_GetPredictedDisplayTime);

	MICROPROFILE_META_CPU("Predict Frame", (int)frameIndex);

	if (session->FrameIndex == 0)
		return ovr_GetTimeInSeconds();

	double predictAhead = vr::VRCompositor()->GetFrameTimeRemaining();
	if (session && frameIndex > 0)
	{
		// Some applications ask for frames ahead of the current frame
		ovrHmdDesc* pHmd = session->Details->HmdDesc;
		predictAhead += double(frameIndex - session->FrameIndex) / pHmd->DisplayRefreshRate;
	}
	return ovr_GetTimeInSeconds() + predictAhead;
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
		return REV_SWAPCHAIN_MAX_LENGTH;

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

	if (strcmp(propertyName, "IPD") == 0)
		return vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserIpdMeters_Float);

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

		// We only know the horizontal depth
		values[0] = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserHeadToEyeDepthMeters_Float);
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
	return ovrError_InvalidOperation;
}
