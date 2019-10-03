#include "OVR_CAPI.h"
#include "OVR_Version.h"
#include "XR_Math.h"

#include "version.h"

#include "Common.h"
#include "Session.h"
#include "InputManager.h"
#include "SwapChain.h"
#include "Extensions.h"

#define XR_USE_GRAPHICS_API_D3D11
#include <d3d11.h>
#include <dxgi1_2.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <Windows.h>
#include <MinHook.h>
#include <list>
#include <vector>
#include <algorithm>
#include <thread>
#include <wrl/client.h>

#define REV_DEFAULT_TIMEOUT 10000

XrInstance g_Instance = XR_NULL_HANDLE;
uint32_t g_MinorVersion = OVR_MINOR_VERSION;
std::list<ovrHmdStruct> g_Sessions;
Extensions g_Extensions;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params)
{
	if (g_Instance)
		return ovrSuccess;

	MicroProfileOnThreadCreate("Main");
	MicroProfileSetForceEnable(true);
	MicroProfileSetEnableAllGroups(true);
	MicroProfileSetForceMetaCounters(true);
	MicroProfileWebServerStart();

	g_MinorVersion = params->RequestedMinorVersion;

	uint32_t size;
	std::vector<XrExtensionProperties> properties;
	CHK_XR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &size, nullptr));
	properties.resize(size);
	for (XrExtensionProperties& props : properties)
		props = XR_TYPE(EXTENSION_PROPERTIES);
	CHK_XR(xrEnumerateInstanceExtensionProperties(nullptr, (uint32_t)properties.size(), &size, properties.data()));
	g_Extensions.InitExtensionList(properties);

	MH_QueueDisableHook(LoadLibraryW);
	MH_QueueDisableHook(OpenEventW);
	MH_ApplyQueued();

	XrInstanceCreateInfo createInfo = g_Extensions.GetInstanceCreateInfo();
	CHK_XR(xrCreateInstance(&createInfo, &g_Instance));

	MH_QueueEnableHook(LoadLibraryW);
	MH_QueueEnableHook(OpenEventW);
	MH_ApplyQueued();

	return ovrSuccess;
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

	ovrHmdDesc desc = { g_MinorVersion < 38 ? ovrHmd_CV1 : ovrHmd_RiftS };
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
		if (g_MinorVersion < 17)
			desc.DefaultEyeFov[i] = OVR::FovPort::Uncant(XR::FovPort(session->DefaultEyeViews[i].fov), XR::Quatf(session->DefaultEyeViews[i].pose.orientation));
		else
			desc.DefaultEyeFov[i] = XR::FovPort(session->DefaultEyeViews[i].fov);
		desc.MaxEyeFov[i] = desc.DefaultEyeFov[i];
		desc.Resolution.w += (int)session->ViewConfigs[i].recommendedImageRectWidth;
		desc.Resolution.h = std::max(desc.Resolution.h, (int)session->ViewConfigs[i].recommendedImageRectHeight);
	}
	desc.DisplayRefreshRate = session->FrameState.predictedDisplayPeriod > 0 ? 1e9f / session->FrameState.predictedDisplayPeriod : 90.0f;
	return desc;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackerCount(ovrSession session)
{
	REV_TRACE(ovr_GetTrackerCount);

	if (!session)
		return ovrError_InvalidSession;

	// Pre-1.37 applications need virtual sensors to avoid a loss of tracking being detected
	return g_MinorVersion < 37 ? 3 : 0;
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

	// Initialize the opaque pointer with our own OpenVR-specific struct
	g_Sessions.emplace_back();

	// Initialize session members
	ovrSession session = &g_Sessions.back();
	session->Instance = g_Instance;
	session->TrackingSpace = XR_REFERENCE_SPACE_TYPE_LOCAL;
	session->SystemProperties = XR_TYPE(SYSTEM_PROPERTIES);
	for (int i = 0; i < ovrEye_Count; i++) {
		session->ViewConfigs[i] = XR_TYPE(VIEW_CONFIGURATION_VIEW);
		session->DefaultEyeViews[i] = XR_TYPE(VIEW);
	}

	// Initialize input
	session->Input.reset(new InputManager(session->Instance));

	XrSystemGetInfo systemInfo = XR_TYPE(SYSTEM_GET_INFO);
	systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	CHK_XR(xrGetSystem(session->Instance, &systemInfo, &session->System));
	CHK_XR(xrGetSystemProperties(session->Instance, session->System, &session->SystemProperties));

	uint32_t numViews;
	CHK_XR(xrEnumerateViewConfigurationViews(session->Instance, session->System, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, ovrEye_Count, &numViews, session->ViewConfigs));
	assert(numViews == ovrEye_Count);

	XrGraphicsRequirementsD3D11KHR graphicsReq = XR_TYPE(GRAPHICS_REQUIREMENTS_D3D11_KHR);
	CHK_XR(xrGetD3D11GraphicsRequirementsKHR(session->Instance, session->System, &graphicsReq));

	// Copy the LUID into the structure
	static_assert(sizeof(graphicsReq.adapterLuid) == sizeof(ovrGraphicsLuid),
		"The adapter LUID needs to fit in ovrGraphicsLuid");
	if (pLuid)
		memcpy(pLuid, &graphicsReq.adapterLuid, sizeof(ovrGraphicsLuid));

	// Create a temporary session to retrieve the headset field-of-view
	Microsoft::WRL::ComPtr<IDXGIFactory1> pFactory = NULL;
	if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory)))
	{
		Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
		Microsoft::WRL::ComPtr<ID3D11Device> pDevice;

		for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 adapterDesc;
			if (SUCCEEDED(pAdapter->GetDesc1(&adapterDesc)) &&
				memcmp(&adapterDesc.AdapterLuid, &graphicsReq.adapterLuid, sizeof(graphicsReq.adapterLuid)) == 0)
			{
				break;
			}
		}

		HRESULT hr = D3D11CreateDevice(pAdapter.Get(),
			D3D_DRIVER_TYPE_UNKNOWN, 0, 0,
			NULL, 0, D3D11_SDK_VERSION,
			&pDevice, nullptr, nullptr);
		assert(SUCCEEDED(hr));

		XrGraphicsBindingD3D11KHR graphicsBinding = XR_TYPE(GRAPHICS_BINDING_D3D11_KHR);
		graphicsBinding.device = pDevice.Get();

		XrSession fakeSession;
		XrSessionCreateInfo createInfo = XR_TYPE(SESSION_CREATE_INFO);
		createInfo.next = &graphicsBinding;
		createInfo.systemId = session->System;
		CHK_XR(xrCreateSession(session->Instance, &createInfo, &fakeSession));

		XrSpace viewSpace;
		XrReferenceSpaceCreateInfo spaceInfo = XR_TYPE(REFERENCE_SPACE_CREATE_INFO);
		spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
		spaceInfo.poseInReferenceSpace = XR::Posef(XR::Posef::Identity());
		CHK_XR(xrCreateReferenceSpace(fakeSession, &spaceInfo, &viewSpace));

		XrSessionBeginInfo beginInfo = XR_TYPE(SESSION_BEGIN_INFO);
		beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		CHK_XR(xrBeginSession(fakeSession, &beginInfo));

		for (int i = 0; i < ovrEye_Count; i++)
			session->DefaultEyeViews[i] = XR_TYPE(VIEW);

		uint32_t numViews;
		XrViewLocateInfo locateInfo = XR_TYPE(VIEW_LOCATE_INFO);
		XrViewState viewState = XR_TYPE(VIEW_STATE);
		locateInfo.space = viewSpace;
		locateInfo.viewConfigurationType = beginInfo.primaryViewConfigurationType;
		locateInfo.displayTime = 1337;
		XrResult rs = xrLocateViews(fakeSession, &locateInfo, &viewState, ovrEye_Count, &numViews, session->DefaultEyeViews);
		assert(XR_SUCCEEDED(rs));
		assert(numViews == ovrEye_Count);

		xrRequestExitSession(fakeSession);
		while (!XR_SUCCEEDED(xrEndSession(fakeSession)))
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		CHK_XR(xrDestroySession(fakeSession));
	}

	*pSession = session;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_Destroy(ovrSession session)
{
	REV_TRACE(ovr_Destroy);

	XrSession handle = session->Session;
	if (handle)
	{
		xrRequestExitSession(session->Session);
		while (!XR_SUCCEEDED(xrEndSession(handle)))
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	if (session->HookedFunction)
		MH_RemoveHook(session->HookedFunction);

	// Delete the session from the list of sessions
	g_Sessions.erase(std::find_if(g_Sessions.begin(), g_Sessions.end(), [session](ovrHmdStruct const& o) { return &o == session; }));

	if (handle)
	{
		XrResult rs = xrDestroySession(handle);
		assert(XR_SUCCEEDED(rs));
	}
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetSessionStatus(ovrSession session, ovrSessionStatus* sessionStatus)
{
	REV_TRACE(ovr_GetSessionStatus);

	if (!session)
		return ovrError_InvalidSession;

	if (!sessionStatus)
		return ovrError_InvalidParameter;

	SessionStatusBits& status = session->SessionStatus;
	XrEventDataBuffer event = XR_TYPE(EVENT_DATA_BUFFER);
	while (xrPollEvent(session->Instance, &event) == XR_SUCCESS)
	{
		switch (event.type)
		{
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			const XrEventDataSessionStateChanged& stateChanged =
				reinterpret_cast<XrEventDataSessionStateChanged&>(event);
			if (stateChanged.session == session->Session)
			{
				if (stateChanged.state == XR_SESSION_STATE_VISIBLE)
				{
					status.HmdMounted = true;
					status.HasInputFocus = false;
				}
				else if (stateChanged.state == XR_SESSION_STATE_IDLE)
					status.HmdPresent = true;
				else if (stateChanged.state == XR_SESSION_STATE_READY)
					status.IsVisible = true;
				else if (stateChanged.state == XR_SESSION_STATE_SYNCHRONIZED)
					status.HmdMounted = false;
				else if (stateChanged.state == XR_SESSION_STATE_FOCUSED)
					status.HasInputFocus = true;
				else if (stateChanged.state == XR_SESSION_STATE_STOPPING)
					status.IsVisible = false;
				else if (stateChanged.state == XR_SESSION_STATE_LOSS_PENDING)
					status.DisplayLost = true;
				else if (stateChanged.state == XR_SESSION_STATE_EXITING)
					status.ShouldQuit = true;
			}
			break;
		}
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
		{
			const XrEventDataInstanceLossPending& lossPending =
				reinterpret_cast<XrEventDataInstanceLossPending&>(event);
			status.ShouldQuit = true;
			break;
		}
		case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
		{
			const XrEventDataReferenceSpaceChangePending& spaceChange =
				reinterpret_cast<XrEventDataReferenceSpaceChangePending&>(event);
			if (spaceChange.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL)
			{
				if (spaceChange.poseValid)
					session->CalibratedOrigin = XR::Posef(session->CalibratedOrigin) * XR::Posef(spaceChange.poseInPreviousSpace);
				status.ShouldRecenter = true;
			}
			break;
		}
		}
	}

	sessionStatus->IsVisible = status.IsVisible;
	sessionStatus->HmdPresent = status.HmdPresent;
	sessionStatus->HmdMounted = status.HmdMounted;
	sessionStatus->DisplayLost = status.DisplayLost;
	sessionStatus->ShouldQuit = status.ShouldQuit;
	sessionStatus->ShouldRecenter = status.ShouldRecenter;
	sessionStatus->HasInputFocus = status.HasInputFocus;
	sessionStatus->OverlayPresent = status.OverlayPresent;
#if 0 // TODO: Re-enable once we figure out why this crashes Arktika.1
	sessionStatus->DepthRequested = g_Extensions.CompositionDepth;
#endif

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetTrackingOriginType(ovrSession session, ovrTrackingOrigin origin)
{
	REV_TRACE(ovr_SetTrackingOriginType);

	if (!session)
		return ovrError_InvalidSession;

	session->TrackingSpace = (XrReferenceSpaceType)(XR_REFERENCE_SPACE_TYPE_LOCAL + origin);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrTrackingOrigin) ovr_GetTrackingOriginType(ovrSession session)
{
	REV_TRACE(ovr_GetTrackingOriginType);

	if (!session)
		return ovrTrackingOrigin_EyeLevel;

	return (ovrTrackingOrigin)(session->TrackingSpace - XR_REFERENCE_SPACE_TYPE_LOCAL);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RecenterTrackingOrigin(ovrSession session)
{
	REV_TRACE(ovr_RecenterTrackingOrigin);

	if (!session)
		return ovrError_InvalidSession;

	XrSpaceLocation relation = XR_TYPE(SPACE_LOCATION);
	CHK_XR(xrLocateSpace(session->ViewSpace, session->LocalSpace, session->FrameState.predictedDisplayTime, &relation));

	if (!(relation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT))
		return ovrError_InvalidHeadsetOrientation;

	return ovr_SpecifyTrackingOrigin(session, XR::Posef(relation.pose));
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SpecifyTrackingOrigin(ovrSession session, ovrPosef originPose)
{
	if (!session)
		return ovrError_InvalidSession;

	// Get a leveled head pose
	float yaw;
	OVR::Quatf(originPose.Orientation).GetYawPitchRoll(&yaw, nullptr, nullptr);
	OVR::Posef newOrigin = OVR::Posef(session->CalibratedOrigin) * OVR::Posef(OVR::Quatf(OVR::Axis_Y, yaw), originPose.Position);
	session->CalibratedOrigin = newOrigin.Normalized();

	XrSpace oldSpace = session->LocalSpace;
	XrReferenceSpaceCreateInfo spaceInfo = XR_TYPE(REFERENCE_SPACE_CREATE_INFO);
	spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	spaceInfo.poseInReferenceSpace = XR::Posef(session->CalibratedOrigin);
	CHK_XR(xrCreateReferenceSpace(session->Session, &spaceInfo, &session->LocalSpace));
	CHK_XR(xrDestroySpace(oldSpace));

	ovr_ClearShouldRecenterFlag(session);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_ClearShouldRecenterFlag(ovrSession session)
{
	session->SessionStatus.ShouldRecenter = false;
}

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingState(ovrSession session, double absTime, ovrBool latencyMarker)
{
	REV_TRACE(ovr_GetTrackingState);

	ovrTrackingState state = { 0 };

	if (session && session->Input)
		session->Input->GetTrackingState(session, &state, absTime);

	return state;
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

		XrSpaceLocation relation = XR_TYPE(SPACE_LOCATION);
		if (XR_SUCCEEDED(xrLocateSpace(session->ViewSpace, session->LocalSpace, session->FrameState.predictedDisplayTime, &relation)))
		{
			// Create a leveled head pose
			if (relation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
			{
				float yaw;
				XR::Posef headPose(relation.pose);
				headPose.Rotation.GetYawPitchRoll(&yaw, nullptr, nullptr);
				headPose.Rotation = OVR::Quatf(OVR::Axis_Y, yaw);
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

	if (!session || !session->Input)
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

	return ovrControllerType_Touch | ovrControllerType_XBox | ovrControllerType_Remote;
}

OVR_PUBLIC_FUNCTION(ovrTouchHapticsDesc) ovr_GetTouchHapticsDesc(ovrSession session, ovrControllerType controllerType)
{
	REV_TRACE(ovr_GetTouchHapticsDesc);

	return InputManager::GetTouchHapticsDesc(controllerType);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	REV_TRACE(ovr_SetControllerVibration);

	if (!session || !session->Input)
		return ovrError_InvalidSession;

	return session->Input->SetControllerVibration(session, controllerType, frequency, amplitude);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitControllerVibration(ovrSession session, ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	REV_TRACE(ovr_SubmitControllerVibration);

	if (!session || !session->Input)
		return ovrError_InvalidSession;

	return session->Input->SubmitControllerVibration(session, controllerType, buffer);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState)
{
	REV_TRACE(ovr_GetControllerVibrationState);

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
		for (int i = 0; i < 4; i++)
		{
			outFloorPoints[i] = (OVR::Vector3f(bounds) / 2.0f);
			if (i % 2 == 0)
				outFloorPoints[i].x *= -1.0f;
			if (i / 2 == 0)
				outFloorPoints[i].z *= -1.0f;
		}
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

	MICROPROFILE_META_CPU("Identifier", (int)chain->Swapchain);
	*out_Length = chain->Length;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainCurrentIndex(ovrSession session, ovrTextureSwapChain chain, int* out_Index)
{
	REV_TRACE(ovr_GetTextureSwapChainCurrentIndex);

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", (int)chain->Swapchain);
	MICROPROFILE_META_CPU("Index", chain->CurrentIndex);
	*out_Index = chain->CurrentIndex;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainDesc(ovrSession session, ovrTextureSwapChain chain, ovrTextureSwapChainDesc* out_Desc)
{
	REV_TRACE(ovr_GetTextureSwapChainDesc);

	if (!chain)
		return ovrError_InvalidParameter;

	MICROPROFILE_META_CPU("Identifier", (int)chain->Swapchain);
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

	MICROPROFILE_META_CPU("Identifier", (int)chain->Swapchain);
	MICROPROFILE_META_CPU("CurrentIndex", chain->CurrentIndex);

	XrSwapchainImageReleaseInfo releaseInfo = XR_TYPE(SWAPCHAIN_IMAGE_RELEASE_INFO);
	CHK_XR(xrReleaseSwapchainImage(chain->Swapchain, &releaseInfo));

	XrSwapchainImageAcquireInfo acquireInfo = XR_TYPE(SWAPCHAIN_IMAGE_ACQUIRE_INFO);
	CHK_XR(xrAcquireSwapchainImage(chain->Swapchain, &acquireInfo, &chain->CurrentIndex));

	XrSwapchainImageWaitInfo waitInfo = XR_TYPE(SWAPCHAIN_IMAGE_WAIT_INFO);
	waitInfo.timeout = session->FrameState.predictedDisplayPeriod;
	CHK_XR(xrWaitSwapchainImage(chain->Swapchain, &waitInfo));

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

	// TODO: Calculate the size from the fov
	// TODO: Add support for pixelsPerDisplayPixel
	ovrSizei size = {
		(int)session->ViewConfigs[eye].recommendedImageRectWidth,
		(int)session->ViewConfigs[eye].recommendedImageRectHeight,
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

	desc.PixelsPerTanAngleAtCenter = OVR::Vector2f(
		desc.DistortedViewport.Size.w / (fov.LeftTan + fov.RightTan),
		desc.DistortedViewport.Size.h / (fov.UpTan + fov.DownTan)
	);

	XR::Posef pose = session->DefaultEyeViews[eyeType].pose;
	if (session->Session)
	{
		uint32_t numViews;
		XrViewLocateInfo locateInfo = XR_TYPE(VIEW_LOCATE_INFO);
		XrViewState viewState = XR_TYPE(VIEW_STATE);
		XrView views[ovrEye_Count] = XR_TYPE(VIEW);
		locateInfo.displayTime = session->FrameState.predictedDisplayTime;
		locateInfo.space = session->ViewSpace;
		XrResult rs = xrLocateViews(session->Session, &locateInfo, &viewState, ovrEye_Count, &numViews, views);
		assert(XR_SUCCEEDED(rs));
		assert(numViews == ovrEye_Count);

		// FIXME: WMR viewState flags are bugged
		if (views[eyeType].pose.orientation.w != 0.0f)
			pose = views[eyeType].pose;
	}

	desc.HmdToEyePose = pose;
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

	if (!session || !session->Session)
		return ovrError_InvalidSession;

	XrFrameWaitInfo waitInfo = XR_TYPE(FRAME_WAIT_INFO);
	session->FrameState = XR_TYPE(FRAME_STATE);
	CHK_XR(xrWaitFrame(session->Session, &waitInfo, &session->FrameState));
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_BeginFrame(ovrSession session, long long frameIndex)
{
	REV_TRACE(ovr_BeginFrame);
	MICROPROFILE_META_CPU("Begin Frame", (int)frameIndex);

	if (!session || !session->Session)
		return ovrError_InvalidSession;

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

	if (!session || !session->Session)
		return ovrError_InvalidSession;

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
		if (g_MinorVersion < 25)
		{
			layer = (ovrLayer_Union*)((char*)layer - sizeof(ovrLayerHeader::Reserved));
		}

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
				view = XR_TYPE(VIEW_CONFIGURATION_VIEW);

				if (type == ovrLayerType_EyeMatrix)
				{
					// RenderPose is the first member that's differently aligned
					view.pose = XR::Posef(layer->EyeMatrix.RenderPose[i]);
					view.fov = XR::Matrix4f(layer->EyeMatrix.Matrix[i]);
				}
				else
				{
					view.pose = XR::Posef(layer->EyeFov.RenderPose[i]);

					// The Climb specifies an invalid fov in the first frame
					XR::FovPort Fov(layer->EyeFov.Fov[i]);
					if (Fov.GetMaxSideTan() > 0.0f)
						view.fov = Fov;
					else
						break;
				}

				if (type == ovrLayerType_EyeFovDepth && g_Extensions.CompositionDepth)
				{
					depthData.emplace_back();
					XrCompositionLayerDepthInfoKHR& depthInfo = depthData.back();
					depthInfo = XR_TYPE(COMPOSITION_LAYER_DEPTH_INFO_KHR);

					depthInfo.subImage.swapchain = layer->EyeFovDepth.DepthTexture[i]->Swapchain;
					depthInfo.subImage.imageRect = XR::Recti(layer->EyeFovDepth.Viewport[i], upsideDown);
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
				view.subImage.imageRect = XR::Recti(layer->EyeFov.Viewport[i], upsideDown);
				view.subImage.imageArrayIndex = 0;
			}

			// Verify all views were initialized without errors
			if (i < ovrEye_Count)
				continue;

			projection.viewCount = ovrEye_Count;
			projection.views = reinterpret_cast<XrCompositionLayerProjectionView*>(&viewData.back());
		}
		else if (type == ovrLayerType_Quad)
		{
			if (!layer->Quad.ColorTexture)
				continue;

			XrCompositionLayerQuad& quad = newLayer.Quad;
			quad = XR_TYPE(COMPOSITION_LAYER_QUAD);
			quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
			quad.subImage.swapchain = layer->Quad.ColorTexture->Swapchain;
			quad.subImage.imageRect = XR::Recti(layer->Quad.Viewport, upsideDown);
			quad.subImage.imageArrayIndex = 0;
			quad.pose = XR::Posef(layer->Quad.QuadPoseCenter);
			quad.size = XR::Vector2f(layer->Quad.QuadSize);
		}
		else if (type == ovrLayerType_Cylinder && g_Extensions.CompositionCylinder)
		{
			if (!layer->Cylinder.ColorTexture)
				continue;

			XrCompositionLayerCylinderKHR& cylinder = newLayer.Cylinder;
			cylinder = XR_TYPE(COMPOSITION_LAYER_CYLINDER_KHR);
			cylinder.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
			cylinder.subImage.swapchain = layer->Cylinder.ColorTexture->Swapchain;
			cylinder.subImage.imageRect = XR::Recti(layer->Cylinder.Viewport, upsideDown);
			cylinder.subImage.imageArrayIndex = 0;
			cylinder.pose = XR::Posef(layer->Cylinder.CylinderPoseCenter);
			cylinder.radius = layer->Cylinder.CylinderRadius;
			cylinder.centralAngle = layer->Cylinder.CylinderAngle;
			cylinder.aspectRatio = layer->Cylinder.CylinderAspectRatio;
		}
		else if (type == ovrLayerType_Cube && g_Extensions.CompositionCube)
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
			continue;
		}

		XrCompositionLayerBaseHeader& header = newLayer.Header;
		header.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		if (headLocked)
			header.space = session->ViewSpace;
		else
			header.space = (session->TrackingSpace == XR_REFERENCE_SPACE_TYPE_STAGE) ?
				session->StageSpace : session->LocalSpace;

		layers.push_back(&newLayer.Header);
	}

	XrFrameEndInfo endInfo = XR_TYPE(FRAME_END_INFO);
	endInfo.displayTime = session->FrameState.predictedDisplayTime;
	endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endInfo.layerCount = (uint32_t)layers.size();
	endInfo.layers = layers.data();
	CHK_XR(xrEndFrame(session->Session, &endInfo));

	MicroProfileFlip();

	if (frameIndex > 0)
		session->NextFrame = frameIndex + 1;
	else
		session->NextFrame++;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame2(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	REV_TRACE(ovr_SubmitFrame);
	MICROPROFILE_META_CPU("Submit Frame", (int)frameIndex);

	if (!session || !session->Session)
		return ovrError_InvalidSession;

	CHK_OVR(ovr_EndFrame(session, frameIndex, viewScaleDesc, layerPtrList, layerCount));
	CHK_OVR(ovr_WaitToBeginFrame(session, session->NextFrame));
	CHK_OVR(ovr_BeginFrame(session, session->NextFrame));
	return ovrSuccess;
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

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_ResetPerfStats(ovrSession session)
{
	REV_TRACE(ovr_ResetPerfStats);

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetPredictedDisplayTime(ovrSession session, long long frameIndex)
{
	REV_TRACE(ovr_GetPredictedDisplayTime);

	MICROPROFILE_META_CPU("Predict Frame", (int)frameIndex);

	XrTime displayTime = session->FrameState.predictedDisplayTime;

	if (frameIndex > 0)
		displayTime += session->FrameState.predictedDisplayPeriod * (session->NextFrame - frameIndex);

	static double PerfFrequencyInverse = 0.0;
	if (PerfFrequencyInverse == 0.0)
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		PerfFrequencyInverse = 1.0 / (double)freq.QuadPart;
	}

	LARGE_INTEGER li;
	if (XR_FAILED(xrConvertTimeToWin32PerformanceCounterKHR(session->Instance, displayTime, &li)))
		return ovr_GetTimeInSeconds();

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
			return XR::Vector3f(session->DefaultEyeViews[ovrEye_Left].pose.position).Distance(
				XR::Vector3f(session->DefaultEyeViews[ovrEye_Right].pose.position)
			);

		if (strcmp(propertyName, "VsyncToNextVsync") == 0)
			return session->FrameState.predictedDisplayPeriod / 1e9f;
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

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetExternalCameras(ovrSession session, ovrExternalCamera* cameras, unsigned int* inoutCameraCount)
{
	// TODO: Support externalcamera.cfg used by the SteamVR Unity plugin
	return ovrError_NoExternalCameraInfo;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetExternalCameraProperties(ovrSession session, const char* name, const ovrCameraIntrinsics* const intrinsics, const ovrCameraExtrinsics* const extrinsics)
{
	return ovrError_NoExternalCameraInfo;
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

static const XrVector2f VisibleRectangle[] = {
	{ 0.0f, 0.0f },
	{ 1.0f, 0.0f },
	{ 1.0f, 1.0f },
	{ 0.0f, 1.0f }
};

static const uint16_t VisibleRectangleIndices[] = {
	0, 1, 2, 0, 2, 3
};

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetFovStencil(
	ovrSession session,
	const ovrFovStencilDesc* fovStencilDesc,
	ovrFovStencilMeshBuffer* meshBuffer)
{
	if (!g_Extensions.VisibilityMask)
		return ovrError_Unsupported;

	if (!session)
		return ovrError_InvalidSession;

	if (fovStencilDesc->StencilType == ovrFovStencil_VisibleRectangle)
	{
		meshBuffer->UsedVertexCount = sizeof(VisibleRectangle) / sizeof(XrVector2f);
		meshBuffer->UsedIndexCount = sizeof(VisibleRectangleIndices) / sizeof(uint16_t);

		if (meshBuffer->AllocVertexCount >= meshBuffer->UsedVertexCount)
			memcpy(meshBuffer->VertexBuffer, VisibleRectangle, sizeof(VisibleRectangle));
		if (meshBuffer->AllocIndexCount >= meshBuffer->UsedIndexCount)
			memcpy(meshBuffer->IndexBuffer, VisibleRectangleIndices, sizeof(VisibleRectangleIndices));
		return ovrSuccess;
	}

	std::vector<uint32_t> indexBuffer;
	if (meshBuffer->AllocIndexCount > 0)
		indexBuffer.resize(meshBuffer->AllocIndexCount);

	XrVisibilityMaskTypeKHR type = (XrVisibilityMaskTypeKHR)(fovStencilDesc->StencilType + 1);
	XrVisibilityMaskKHR mask = XR_TYPE(VISIBILITY_MASK_KHR);
	mask.vertexCapacityInput = meshBuffer->AllocVertexCount;
	mask.vertices = (XrVector2f*)meshBuffer->VertexBuffer;
	mask.indexCapacityInput = meshBuffer->AllocIndexCount;
	mask.indices = meshBuffer->IndexBuffer ? indexBuffer.data() : nullptr;
	CHK_XR(xrGetVisibilityMaskKHR(session->Session, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, fovStencilDesc->Eye, type, &mask));
	meshBuffer->UsedVertexCount = mask.vertexCountOutput;
	meshBuffer->UsedIndexCount = mask.indexCountOutput;

	if (meshBuffer->VertexBuffer && !(fovStencilDesc->StencilFlags & ovrFovStencilFlag_MeshOriginAtBottomLeft))
	{
		for (int i = 0; i < meshBuffer->AllocVertexCount; i++)
			meshBuffer->VertexBuffer[i].y = 1.0f - meshBuffer->VertexBuffer[i].y;
	}

	if (meshBuffer->IndexBuffer)
	{
		for (int i = 0; i < meshBuffer->AllocIndexCount; i++)
			meshBuffer->IndexBuffer[i] = (uint16_t)indexBuffer[i];
	}

	return ovrSuccess;
}
