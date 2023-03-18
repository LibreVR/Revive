#include "OVR_CAPI.h"
#include "OVR_Version.h"
#include "REV_Math.h"

#include "Common.h"
#include "Session.h"
#include "CompositorBase.h"
#include "InputManager.h"
#include "ProfileManager.h"

#include <dxgi1_2.h>
#include <openvr.h>
#include <Windows.h>
#include <detours/detours.h>
#include <list>
#include <algorithm>
#include <thread>
#include <assert.h>

#define REV_DEFAULT_TIMEOUT 10000

unsigned int g_MinorVersion = OVR_MINOR_VERSION;
vr::EVRInitError g_InitError = vr::VRInitError_Init_NotInitialized;
std::list<ovrHmdStruct> g_Sessions;

#if MICROPROFILE_ENABLED
ProfileManager g_ProfileManager;
#endif

ovrResult InitErrorToOvrError(vr::EVRInitError error)
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
	if (g_InitError == vr::VRInitError_None)
		return ovrSuccess;

#if 0
	LoadRenderDoc();
#endif

	MicroProfileOnThreadCreate("Main");
	MicroProfileSetForceEnable(true);
	MicroProfileSetEnableAllGroups(true);
	MicroProfileSetForceMetaCounters(true);
	MicroProfileWebServerStart();

	g_MinorVersion = params->RequestedMinorVersion;

	DetachDetours();

	vr::VR_Init(&g_InitError, vr::VRApplication_Scene);

	AttachDetours();

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

#if MICROPROFILE_ENABLED
	g_ProfileManager.Initialize();
#endif

	return InitErrorToOvrError(g_InitError);
}

OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{
#if MICROPROFILE_ENABLED
	g_ProfileManager.Shutdown();
#endif

	g_Sessions.clear();
	vr::VR_Shutdown();
	MicroProfileShutdown();
	g_InitError = vr::VRInitError_Init_NotInitialized;
}

OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo)
{
	REV_TRACE(ovr_GetLastErrorInfo);

	if (!errorInfo)
		return;

	const char* error = VR_GetVRInitErrorAsEnglishDescription(g_InitError);
	strcpy_s(errorInfo->ErrorString, sizeof(ovrErrorInfo::ErrorString), error);
	errorInfo->Result = InitErrorToOvrError(g_InitError);
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

	return session->HmdDesc;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackerCount(ovrSession session)
{
	REV_TRACE(ovr_GetTrackerCount);

	if (!session)
		return ovrError_InvalidSession;

	return (unsigned int)session->TrackerCount;
}

OVR_PUBLIC_FUNCTION(ovrTrackerDesc) ovr_GetTrackerDesc(ovrSession session, unsigned int trackerDescIndex)
{
	REV_TRACE(ovr_GetTrackerDesc);

	if (session && trackerDescIndex < session->TrackerCount)
		return session->TrackerDesc[trackerDescIndex];
	return ovrTrackerDesc();
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

	// Oculus games expect a seated tracking space by default
	vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseSeated);

	*pSession = &g_Sessions.back();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_Destroy(ovrSession session)
{
	REV_TRACE(ovr_Destroy);

	// Delete the session from the list of sessions
	g_Sessions.erase(std::find_if(g_Sessions.begin(), g_Sessions.end(), [session](ovrHmdStruct const& o) { return &o == session; }));
}

typedef struct ovrSessionStatus1_ {
	ovrBool IsVisible;
	ovrBool HmdPresent;
	ovrBool HmdMounted;
	ovrBool DisplayLost;
	ovrBool ShouldQuit;
	ovrBool ShouldRecenter;
	ovrBool HasInputFocus;
	ovrBool OverlayPresent;
} ovrSessionStatus1;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetSessionStatus(ovrSession session, ovrSessionStatus* sessionStatus)
{
	REV_TRACE(ovr_GetSessionStatus);

	if (!session)
		return ovrError_InvalidSession;

	if (!sessionStatus)
		return ovrError_InvalidParameter;

	session->UpdateStatus();

	if (g_MinorVersion > 20)
		*sessionStatus = session->Status;
	else
		memcpy(sessionStatus, &session->Status, sizeof(ovrSessionStatus1));

	// Detect if the application has focus, but return false the first time the status is requested.
	// If this is true from the first call then Airmech will assume the Health-and-Safety warning
	// is still being displayed.
	static bool first_call = true;
	sessionStatus->IsVisible = vr::VRCompositor()->CanRenderScene() && !first_call;
	first_call = false;

	static const bool do_sleep = session->UseHack(HACK_SLEEP_IN_SESSION_STATUS);
	if (do_sleep)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetTrackingOriginType(ovrSession session, ovrTrackingOrigin origin)
{
	REV_TRACE(ovr_SetTrackingOriginType);

	if (!session)
		return ovrError_InvalidSession;

	// Both enums match exactly, so we can just cast them
	vr::VRCompositor()->SetTrackingSpace((vr::ETrackingUniverseOrigin)origin);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrTrackingOrigin) ovr_GetTrackingOriginType(ovrSession session)
{
	REV_TRACE(ovr_GetTrackingOriginType);

	if (!session)
		return ovrTrackingOrigin_EyeLevel;

	// Both enums match exactly, so we can just cast them
	return (ovrTrackingOrigin)vr::VRCompositor()->GetTrackingSpace();
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RecenterTrackingOrigin(ovrSession session)
{
	REV_TRACE(ovr_RecenterTrackingOrigin);

	if (!session)
		return ovrError_InvalidSession;

	vr::VRChaperone()->ResetZeroPose(vr::VRCompositor()->GetTrackingSpace());
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SpecifyTrackingOrigin(ovrSession session, ovrPosef originPose)
{
	vr::ChaperoneCalibrationState calibrationState = vr::VRChaperone()->GetCalibrationState();
	if (calibrationState >= vr::ChaperoneCalibrationState_Error)
		return ovrSuccess_BoundaryInvalid;

	float yaw = 0.0f;
	OVR::Posef(originPose).Rotation.GetYawPitchRoll(&yaw, nullptr, nullptr);
	if (yaw == 0.0f && OVR::Posef(originPose).Rotation != OVR::Quatf::Identity())
		return ovrError_InvalidParameter;

	vr::HmdMatrix34_t workingPose;
	vr::ETrackingUniverseOrigin origin = vr::VRCompositor()->GetTrackingSpace();
	vr::VRChaperoneSetup()->RevertWorkingCopy();
	if (origin == vr::TrackingUniverseOrigin::TrackingUniverseSeated)
		vr::VRChaperoneSetup()->GetWorkingSeatedZeroPoseToRawTrackingPose(&workingPose);
	else
		vr::VRChaperoneSetup()->GetWorkingStandingZeroPoseToRawTrackingPose(&workingPose);

	workingPose = REV::Matrix4f(OVR::Matrix4f::RotationY(yaw) * REV::Matrix4f(workingPose) *
		OVR::Matrix4f::Translation(originPose.Position));

	if (origin == vr::TrackingUniverseOrigin::TrackingUniverseSeated)
		vr::VRChaperoneSetup()->SetWorkingSeatedZeroPoseToRawTrackingPose(&workingPose);
	else
		vr::VRChaperoneSetup()->SetWorkingStandingZeroPoseToRawTrackingPose(&workingPose);
	vr::VRChaperoneSetup()->CommitWorkingCopy(vr::EChaperoneConfigFile_Live);
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

	if (session->UseHack(HACK_SPOOF_SENSORS))
	{
		if (trackerPoseIndex < ovr_GetTrackerCount(session))
		{
			const OVR::Posef poses[] = {
				OVR::Posef(OVR::Quatf(OVR::Axis_Y, OVR::DegreeToRad(90.0f)), OVR::Vector3f(-2.0f, 0.0f, 0.2f)),
				OVR::Posef(OVR::Quatf(OVR::Axis_Y, OVR::DegreeToRad(0.0f)), OVR::Vector3f(-0.2f, 0.0f, -2.0f)),
				OVR::Posef(OVR::Quatf(OVR::Axis_Y, OVR::DegreeToRad(180.0f)), OVR::Vector3f(0.2f, 0.0f, 2.0f))
			};
			OVR::Posef trackerPose = poses[trackerPoseIndex];

			vr::TrackedDevicePose_t pose;
			vr::VRCompositor()->GetLastPoseForTrackedDeviceIndex(vr::k_unTrackedDeviceIndex_Hmd, &pose, nullptr);

			// Create a leveled head pose
			if (pose.bPoseIsValid)
			{
				float yaw;
				REV::Matrix4f matrix(pose.mDeviceToAbsoluteTracking);
				OVR::Quatf headOrientation(matrix);
				headOrientation.GetYawPitchRoll(&yaw, nullptr, nullptr);
				headOrientation = OVR::Quatf(OVR::Axis_Y, yaw);
				trackerPose = OVR::Posef(headOrientation, matrix.GetTranslation()) * trackerPose;
			}

			tracker.Pose = trackerPose;
			tracker.LeveledPose = trackerPose;
			tracker.TrackerFlags = ovrTracker_Connected | ovrTracker_PoseTracked;
		}
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

	if (chain->Length > 1 && chain->Full())
		return ovrError_TextureSwapChainFull;

	chain->Commit();

	if (chain->Overlay != vr::k_ulOverlayHandleInvalid)
	{
		// Submit overlays on commit so we don't upload textures every frame
		vr::Texture_t texture;
		chain->Submit()->ToVRTexture(texture);
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
	const ovrEyeRenderDesc& desc = session->RenderDesc[eye];
	ovrSizei size = desc.DistortedViewport.Size;

	// Grow the recommended size to account for the overlapping fov
	// TODO: Add a setting to ignore pixelsPerDisplayPixel
	vr::VRTextureBounds_t bounds = CompositorBase::FovPortToTextureBounds(desc.Fov, fov);
	size.w = int(size.w / (bounds.uMax - bounds.uMin));
	size.h = int(size.h / (bounds.vMax - bounds.vMin));

	return size;
}

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovr_GetRenderDesc2(ovrSession session, ovrEyeType eyeType, ovrFovPort fov)
{
	REV_TRACE(ovr_GetRenderDesc);

	// Make a copy so we can adjust a few parameters
	ovrEyeRenderDesc desc = session->RenderDesc[eyeType];

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

	session->Compositor->SetTimingMode(vr::VRCompositorTimingMode_Explicit_ApplicationPerformsPostPresentHandoff);
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
	return session->Compositor->EndFrame(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
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
	session->Compositor->SetTimingMode(vr::VRCompositorTimingMode_Implicit);
	ovrResult result = session->Compositor->EndFrame(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
	if (OVR_SUCCESS(result))
	{
		// Begin the next frame
		session->Compositor->WaitToBeginFrame(session, frameIndex + 1);
		session->Compositor->BeginFrame(session, frameIndex + 1);
	}
	return result;
}

typedef struct OVR_ALIGNAS(4) ovrViewScaleDesc1_ {
	ovrVector3f HmdToEyeOffset[ovrEye_Count]; ///< Translation of each eye.
	float HmdSpaceToWorldScaleInMeters; ///< Ratio of viewer units to meter units.
} ovrViewScaleDesc1;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc1* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	if (viewScaleDesc)
	{
		ovrViewScaleDesc viewScale;
		viewScale.HmdToEyePose[ovrEye_Left] = OVR::Posef(session->RenderDesc[ovrEye_Left].HmdToEyePose.Orientation, viewScaleDesc->HmdToEyeOffset[ovrEye_Left]);
		viewScale.HmdToEyePose[ovrEye_Right] = OVR::Posef(session->RenderDesc[ovrEye_Right].HmdToEyePose.Orientation, viewScaleDesc->HmdToEyeOffset[ovrEye_Right]);
		viewScale.HmdSpaceToWorldScaleInMeters = viewScaleDesc->HmdSpaceToWorldScaleInMeters;
		return ovr_SubmitFrame2(session, frameIndex, &viewScale, layerPtrList, layerCount);
	}
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

	if (session->UseHack(HACK_DISABLE_STATS))
		return ovrSuccess;

	ovrPerfStatsPerCompositorFrame FrameStats[ovrMaxProvidedFrameStats] = { 0 };

	// TODO: Implement performance scale heuristics
	float AdaptiveGpuPerformanceScale = 1.0f;
	ovrBool AnyFrameStatsDropped = (session->FrameIndex - session->StatsIndex) > ovrMaxProvidedFrameStats;
	int FrameStatsCount = AnyFrameStatsDropped ? ovrMaxProvidedFrameStats : int(session->FrameIndex - session->StatsIndex);
	session->StatsIndex = session->FrameIndex;

	vr::Compositor_FrameTiming TimingStats[ovrMaxProvidedFrameStats];
	TimingStats[0].m_nSize = sizeof(vr::Compositor_FrameTiming);
	FrameStatsCount = vr::VRCompositor()->GetFrameTimings(TimingStats, FrameStatsCount);

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
		stats.AppDroppedFrameCount = TotalStats.m_nNumDroppedFrames + TotalStats.m_nNumReprojectedFrames;
		// TODO: Improve latency stats with sensor timestamps and latency markers
		stats.AppMotionToPhotonLatency = fFrameDuration + fVsyncToPhotons;
		stats.AppQueueAheadTime = VR_COMPOSITOR_ADDITIONAL_PREDICTED_FRAMES(TimingStats[i]) / fDisplayFrequency;
		stats.AppCpuElapsedTime = TimingStats[i].m_flClientFrameIntervalMs / 1000.0f;
		stats.AppGpuElapsedTime = TimingStats[i].m_flPreSubmitGpuMs / 1000.0f;

		stats.CompositorFrameIndex = TimingStats[i].m_nNumFramePresents;
		stats.CompositorDroppedFrameCount = 0;
		stats.CompositorLatency = fVsyncToPhotons;
		stats.CompositorCpuElapsedTime = TimingStats[i].m_flCompositorRenderCpuMs / 1000.0f;
		stats.CompositorGpuElapsedTime = TimingStats[i].m_flCompositorRenderGpuMs / 1000.0f;
		stats.CompositorCpuStartToGpuEndElapsedTime = (TimingStats[i].m_flCompositorUpdateEndMs - TimingStats[i].m_flCompositorRenderStartMs) / 1000.0f;
		stats.CompositorGpuEndToVsyncElapsedTime = TimingStats[i].m_flCompositorIdleCpuMs / 1000.0f;

		stats.AswIsActive = TimingStats[i].m_nReprojectionFlags & vr::VRCompositor_ReprojectionMotion;
		stats.AswActivatedToggleCount = 0;
		stats.AswPresentedFrameCount = TotalStats.m_nNumReprojectedFrames;
		stats.AswFailedFrameCount = TotalStats.m_nNumDroppedFrames;
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
		out->AswIsAvailable = vr::VRCompositor()->IsMotionSmoothingSupported();

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

	uint32_t predictionID = 0;
	vr::VRCompositor()->GetLastPosePredictionIDs(&predictionID, nullptr);
	predictionID += (uint32_t)(frameIndex - session->FrameIndex);
	return (double)predictionID / session->HmdDesc.DisplayRefreshRate;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds()
{
	REV_TRACE(ovr_GetTimeInSeconds);

	float freq = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
	float elapsed = 1.0f / freq - vr::VRCompositor()->GetFrameTimeRemaining();

	uint32_t predictionID = 0;
	vr::VRCompositor()->GetLastPosePredictionIDs(&predictionID, nullptr);
	return (double)predictionID / freq + elapsed;
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

	if (session)
	{
		vr::EVRSettingsError error = vr::VRSettingsError_None;
		int prop = vr::VRSettings()->GetInt32(session->AppKey, propertyName, &error);
		if (error == vr::VRSettingsError_None)
			return prop;
	}
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
	else if (strcmp(propertyName, "VsyncToNextVsync") == 0)
		return 1.0f / vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
	else if (strcmp(propertyName, "CpuStartToGpuEndSeconds") == 0)
	{
		vr::Compositor_FrameTiming timing;
		timing.m_nSize = sizeof(timing);
		if (vr::VRCompositor()->GetFrameTiming(&timing))
			return (timing.m_flClientFrameIntervalMs + timing.m_flTotalRenderGpuMs) / 1000.0f;
	}

	// Override defaults, we should always return a valid value for these
	if (strcmp(propertyName, OVR_KEY_PLAYER_HEIGHT) == 0)
		defaultVal = OVR_DEFAULT_PLAYER_HEIGHT;
	else if (strcmp(propertyName, OVR_KEY_EYE_HEIGHT) == 0)
		defaultVal = OVR_DEFAULT_EYE_HEIGHT;

	if (session)
	{
		vr::EVRSettingsError error = vr::VRSettingsError_None;
		float prop = vr::VRSettings()->GetFloat(session->AppKey, propertyName);
		if (error = vr::VRSettingsError_None)
			return prop;
	}
	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloat(ovrSession session, const char* propertyName, float value)
{
	REV_TRACE(ovr_SetFloat);

	if (!session)
		return false;

	vr::EVRSettingsError error = vr::VRSettingsError_None;
	vr::VRSettings()->SetFloat(session->AppKey, propertyName, value);
	return error == vr::VRSettingsError_None;
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

	if (!session)
		return 0;

	unsigned int valuesFound = 0;
	char key[vr::k_unMaxSettingsKeyLength] = { 0 };
	for (vr::EVRSettingsError error = vr::VRSettingsError_None; error != vr::VRSettingsError_None && valuesFound < valuesCapacity; valuesFound++)
	{
		sprintf_s(key, "%s[%d]", propertyName, valuesFound);
		values[valuesFound] = vr::VRSettings()->GetFloat(session->AppKey, key, &error);
		if (error != vr::VRSettingsError_None)
			break;
	}
	return valuesFound;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloatArray(ovrSession session, const char* propertyName, const float values[], unsigned int valuesSize)
{
	REV_TRACE(ovr_SetFloatArray);

	if (!session)
		return false;

	char key[vr::k_unMaxSettingsKeyLength] = { 0 };
	for (unsigned int i = 0; i < valuesSize; i++)
	{
		vr::EVRSettingsError error = vr::VRSettingsError_None;
		sprintf_s(key, "%s[%d]", propertyName, i);
		vr::VRSettings()->SetFloat(session->AppKey, key, values[i], &error);
		if (error != vr::VRSettingsError_None)
			return false;
	}
	return true;
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetString(ovrSession session, const char* propertyName, const char* defaultVal)
{
	REV_TRACE(ovr_GetString);

	if (!session)
		return defaultVal;

	// Override defaults, we should always return a valid value for these
	if (strcmp(propertyName, OVR_KEY_GENDER) == 0)
		defaultVal = OVR_DEFAULT_GENDER;

	if (session)
	{
		vr::EVRSettingsError error = vr::VRSettingsError_None;
		vr::VRSettings()->GetString(session->AppKey, propertyName, session->StringBuffer, sizeof(session->StringBuffer), &error);
		if (error != vr::VRSettingsError_None)
			return session->StringBuffer;
	}
	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetString(ovrSession session, const char* propertyName, const char* value)
{
	REV_TRACE(ovr_SetString);

	if (!session)
		return false;

	vr::EVRSettingsError error = vr::VRSettingsError_None;
	vr::VRSettings()->SetString(session->AppKey, propertyName, value, &error);
	return error != vr::VRSettingsError_None;
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

static const ovrVector2f StencilVertices[] = {
	{ 0.0f, 0.0f },
	{ 1.0f, 0.0f },
	{ 1.0f, 1.0f },
	{ 0.0f, 1.0f }
};

static const ovrVector2f InvertedStencilVertices[] = {
	{ 0.0f, 1.0f },
	{ 1.0f, 1.0f },
	{ 1.0f, 0.0f },
	{ 0.0f, 0.0f }
};

static const uint16_t StencilIndices[][6] = {
	{ 0, 0, 0 },
	{ 0, 1, 2, 0, 2, 3 },
	{ 0, 1, 2, 3 },
	{ 0, 1, 2, 0, 2, 3 }
};

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetFovStencil(
	ovrSession session,
	const ovrFovStencilDesc* fovStencilDesc,
	ovrFovStencilMeshBuffer* meshBuffer)
{
	switch (fovStencilDesc->StencilType)
	{
	case ovrFovStencil_HiddenArea:
		meshBuffer->UsedIndexCount = 3;
		meshBuffer->UsedVertexCount = 1;
		break;
	case ovrFovStencil_BorderLine:
		meshBuffer->UsedIndexCount = 4;
		meshBuffer->UsedVertexCount = 4;
		break;
	case ovrFovStencil_VisibleArea:
	case ovrFovStencil_VisibleRectangle:
		meshBuffer->UsedIndexCount = 6;
		meshBuffer->UsedVertexCount = 4;
		break;
	}

	vr::HiddenAreaMesh_t mesh = { nullptr, 0 };
	if (fovStencilDesc->StencilType < vr::k_eHiddenAreaMesh_Max)
		mesh = vr::VRSystem()->GetHiddenAreaMesh((vr::EVREye)fovStencilDesc->Eye, (vr::EHiddenAreaMeshType)fovStencilDesc->StencilType);

	if (mesh.unTriangleCount > 0)
	{
		meshBuffer->UsedIndexCount = mesh.unTriangleCount * 3;
		meshBuffer->UsedVertexCount = mesh.unTriangleCount * 3;
	}

	if (meshBuffer->AllocVertexCount < meshBuffer->UsedVertexCount || meshBuffer->AllocIndexCount < meshBuffer->UsedIndexCount ||
		!meshBuffer->VertexBuffer || !meshBuffer->IndexBuffer)
		return !meshBuffer->AllocVertexCount && !meshBuffer->AllocVertexCount ? ovrSuccess : ovrError_InvalidParameter;

	if (mesh.unTriangleCount == 0)
	{
		const ovrVector2f* vertex = fovStencilDesc->StencilFlags & ovrFovStencilFlag_MeshOriginAtBottomLeft ? InvertedStencilVertices : StencilVertices;
		const uint16_t* index = StencilIndices[fovStencilDesc->StencilType];
		memcpy(meshBuffer->VertexBuffer, vertex, meshBuffer->UsedVertexCount * sizeof(ovrVector2f));
		memcpy(meshBuffer->IndexBuffer, index, meshBuffer->UsedIndexCount * sizeof(uint16_t));
		return ovrSuccess;
	}

	for (uint32_t i = 0; i < mesh.unTriangleCount * 3; i++)
	{
		if (fovStencilDesc->StencilFlags & ovrFovStencilFlag_MeshOriginAtBottomLeft)
			meshBuffer->VertexBuffer[i] = REV::Vector2f(mesh.pVertexData[i]);
		else
			meshBuffer->VertexBuffer[i] = OVR::Vector2f(mesh.pVertexData[i].v[0], 1.0f - mesh.pVertexData[i].v[1]);
		meshBuffer->IndexBuffer[i] = i;
	}
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

OVR_PUBLIC_FUNCTION(ovrHmdColorDesc)
ovr_GetHmdColorDesc(ovrSession session)
{
	ovrHmdColorDesc desc = { ovrColorSpace_Unknown };
	return desc;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_SetClientColorDesc(ovrSession session, const ovrHmdColorDesc* colorDesc)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetControllerPositionUncertainty()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetCurrentHandInputState()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetHandMesh()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetHandPose()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetHandSkeleton()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_ReportClientInfo()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_ReportCompilerInfo()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_EnumerateInstanceExtensionProperties()
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetInstanceProcAddr()
{
	return ovrError_Unsupported;
}
