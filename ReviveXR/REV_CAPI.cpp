#include "OVR_CAPI.h"
#include "OVR_Version.h"
#include "XR_Math.h"

#include "version.h"

#include "Common.h"
#include "Session.h"
#include "InputManager.h"
#include "SwapChain.h"

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

XrInstance g_Instance;
uint32_t g_MinorVersion = OVR_MINOR_VERSION;
std::list<ovrHmdStruct> g_Sessions;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params)
{
	MicroProfileOnThreadCreate("Main");
	MicroProfileSetForceEnable(true);
	MicroProfileSetEnableAllGroups(true);
	MicroProfileSetForceMetaCounters(true);
	MicroProfileWebServerStart();

	g_MinorVersion = params->RequestedMinorVersion;

	std::vector<const char*> extensions;
	extensions.push_back(XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME);
	extensions.push_back("XR_KHR_D3D11_enable");

	XrInstanceCreateInfo createInfo = XR_TYPE(INSTANCE_CREATE_INFO);
	createInfo.applicationInfo = { "Revive", REV_VERSION_INT, "Revive", REV_VERSION_INT, XR_CURRENT_API_VERSION };
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.enabledExtensionNames = extensions.data();

	MH_QueueDisableHook(LoadLibraryW);
	MH_QueueDisableHook(OpenEventW);
	MH_ApplyQueued();

	CHK_XR(xrCreateInstance(&createInfo, &g_Instance));

	MH_QueueEnableHook(LoadLibraryW);
	MH_QueueEnableHook(OpenEventW);
	MH_ApplyQueued();

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{
	for (ovrHmdStruct& session : g_Sessions)
	{
		xrDestroySession(session.Session);
	}
	g_Sessions.clear();

	xrDestroyInstance(g_Instance);
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

	ovrHmdDesc desc = {};
	if (!session)
	{
		desc.Type = ovrHmd_CV1;
		return desc;
	}

	XrInstanceProperties props = XR_TYPE(INSTANCE_PROPERTIES);
	xrGetInstanceProperties(session->Instance, &props);

	strcpy_s(desc.ProductName, 64, "Oculus Rift");
	strcpy_s(desc.Manufacturer, 64, props.runtimeName);

	if (session->SystemProperties.trackingProperties.orientationTracking)
		desc.AvailableTrackingCaps |= ovrTrackingCap_Orientation;
	if (session->SystemProperties.trackingProperties.positionTracking)
		desc.AvailableTrackingCaps |= ovrTrackingCap_Orientation;
	desc.DefaultTrackingCaps = desc.AvailableTrackingCaps;

	for (int i = 0; i < ovrEye_Count; i++)
	{
		desc.DefaultEyeFov[i] = XR::FovPort(session->DefaultEyeFov[i]);
		desc.MaxEyeFov[i] = XR::FovPort(session->DefaultEyeFov[i]);
		desc.Resolution.w += (int)session->ViewConfigs[i].recommendedImageRectWidth;
		desc.Resolution.h = std::max(desc.Resolution.h, (int)session->ViewConfigs[i].recommendedImageRectHeight);
	}
	desc.DisplayRefreshRate = 90.0f;
	return desc;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackerCount(ovrSession session)
{
	REV_TRACE(ovr_GetTrackerCount);

	if (!session)
		return ovrError_InvalidSession;

	return 2;
}

OVR_PUBLIC_FUNCTION(ovrTrackerDesc) ovr_GetTrackerDesc(ovrSession session, unsigned int trackerDescIndex)
{
	REV_TRACE(ovr_GetTrackerDesc);

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

	ovrSession session = &g_Sessions.back();
	session->Instance = g_Instance;

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
			D3D_DRIVER_TYPE_UNKNOWN, 0,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT,
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

		uint32_t numViews;
		XrViewLocateInfo locateInfo = XR_TYPE(VIEW_LOCATE_INFO);
		XrViewState viewState = XR_TYPE(VIEW_STATE);
		XrView views[ovrEye_Count] = XR_TYPE(VIEW);
		locateInfo.space = viewSpace;
		XrResult rs = xrLocateViews(fakeSession, &locateInfo, &viewState, ovrEye_Count, &numViews, views);
		assert(XR_SUCCEEDED(rs));
		assert(numViews == ovrEye_Count);

		for (int i = 0; i < ovrEye_Count; i++)
			session->DefaultEyeFov[i] = views[i].fov;

		CHK_XR(xrEndSession(fakeSession));
		CHK_XR(xrDestroySession(fakeSession));
	}

	*pSession = session;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_Destroy(ovrSession session)
{
	REV_TRACE(ovr_Destroy);

	XrSession backup = session->Session;
	if (backup)
	{
		XrResult rs = xrEndSession(backup);
		assert(XR_SUCCEEDED(rs));
	}

	// Delete the session from the list of sessions
	g_Sessions.erase(std::find_if(g_Sessions.begin(), g_Sessions.end(), [session](ovrHmdStruct const& o) { return &o == session; }));

	if (backup)
	{
		XrResult rs = xrDestroySession(backup);
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

	SessionStatusBits status = session->SessionStatus;

	// Detect if the application has focus, but only return false the first time the status is requested.
	// If this is true from the first call then Airmech will assume the Health-and-Safety warning
	// is still being displayed.
	static bool first_call = true;
	sessionStatus->IsVisible = !first_call;//status.IsVisible && !first_call;
	first_call = false;

	// Don't use the activity level while debugging, so I don't have to put on the HMD
	sessionStatus->HmdPresent = status.HmdPresent;
	sessionStatus->HmdMounted = status.HmdMounted;

	// TODO: Detect if the display is lost, can this ever happen with OpenVR?
	sessionStatus->DisplayLost = status.DisplayLost;
	sessionStatus->ShouldQuit = status.ShouldQuit;
	sessionStatus->ShouldRecenter = status.ShouldRecenter;
	sessionStatus->HasInputFocus = status.HasInputFocus;
	sessionStatus->OverlayPresent = status.OverlayPresent;
	sessionStatus->DepthRequested = true;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetTrackingOriginType(ovrSession session, ovrTrackingOrigin origin)
{
	REV_TRACE(ovr_SetTrackingOriginType);

	if (!session || !session->Session)
		return ovrError_InvalidSession;

	session->TrackingSpace = (XrReferenceSpaceType)(XR_REFERENCE_SPACE_TYPE_LOCAL + origin);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrTrackingOrigin) ovr_GetTrackingOriginType(ovrSession session)
{
	REV_TRACE(ovr_GetTrackingOriginType);

	if (!session)
		return ovrTrackingOrigin_EyeLevel;

	// Both enums match exactly, so we can just cast them
	return (ovrTrackingOrigin)session->TrackingSpace;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RecenterTrackingOrigin(ovrSession session)
{
	REV_TRACE(ovr_RecenterTrackingOrigin);

	if (!session)
		return ovrError_InvalidSession;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SpecifyTrackingOrigin(ovrSession session, ovrPosef originPose)
{
	// TODO: Implement through 
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

	tracker.LeveledPose = OVR::Posef::Identity();
	tracker.Pose = OVR::Posef::Identity();
	tracker.TrackerFlags = ovrTracker_Connected;
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

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_TestBoundaryPoint(ovrSession session, const ovrVector3f* point,
	ovrBoundaryType singleBoundaryType, ovrBoundaryTestResult* outTestResult)
{
	REV_TRACE(ovr_TestBoundaryPoint);

	return ovrError_Unsupported;
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

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryDimensions(ovrSession session, ovrBoundaryType boundaryType, ovrVector3f* outDimensions)
{
	REV_TRACE(ovr_GetBoundaryDimensions);

	return ovrError_Unsupported;
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

	if (!session || !session->Session)
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
	XR::Posef pose(views[eyeType].pose);
	if (pose.Rotation.IsNormalized())
		desc.HmdToEyePose = pose;
	else
		desc.HmdToEyePose = OVR::Posef::Identity();
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
};

OVR_PUBLIC_FUNCTION(ovrResult) ovr_EndFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	REV_TRACE(ovr_EndFrame);
	MICROPROFILE_META_CPU("End Frame", (int)frameIndex);

	if (!session || !session->Session)
		return ovrError_InvalidSession;

	std::vector<XrCompositionLayerUnion> layerData;
	std::vector<XrCompositionLayerBaseHeader*> layers;
	std::vector<XrCompositionLayerProjectionView> views;
	for (unsigned int i = 0; i < layerCount; i++)
	{
		ovrLayer_Union* layer = (ovrLayer_Union*)layerPtrList[i];

		if (!layer)
			continue;

		layerData.emplace_back();
		XrCompositionLayerUnion& newLayer = layerData.back();
		layers.push_back(&newLayer.Header);

		if (layer->Header.Type == ovrLayerType_EyeFov || layer->Header.Type == ovrLayerType_EyeMatrix || layer->Header.Type == ovrLayerType_EyeFovDepth)
		{
			XrCompositionLayerProjection& projection = newLayer.Projection;
			projection = XR_TYPE(COMPOSITION_LAYER_PROJECTION);

			for (int i = 0; i < ovrEye_Count; i++)
			{
				XrCompositionLayerProjectionView view = XR_TYPE(VIEW_CONFIGURATION_VIEW);
				if (layer->Header.Type == ovrLayerType_EyeMatrix)
				{
					// RenderPose is the first member that's differently aligned
					view.pose = XR::Posef(layer->EyeMatrix.RenderPose[i]);
					view.fov = XR::Matrix4f(layer->EyeMatrix.Matrix[i]);
				}
				else
				{
					view.pose = XR::Posef(layer->EyeFov.RenderPose[i]);
					view.fov = XR::FovPort(layer->EyeFov.Fov[i]);
				}
				view.subImage.swapchain = layer->EyeFov.ColorTexture[i]->Swapchain;
				view.subImage.imageRect = XR::Recti(layer->EyeFov.Viewport[i]);
				view.subImage.imageArrayIndex = 0;
				views.push_back(view);
			}

			projection.viewCount = ovrEye_Count;
			projection.views = &views.back() - 1;
		}
		else if (layer->Header.Type == ovrLayerType_Quad)
		{
			XrCompositionLayerQuad& quad = newLayer.Quad;
			quad = XR_TYPE(COMPOSITION_LAYER_QUAD);
			quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
			quad.subImage.swapchain = layer->Quad.ColorTexture->Swapchain;
			quad.subImage.imageRect = XR::Recti(layer->Quad.Viewport);
			quad.subImage.imageArrayIndex = 0;
			quad.pose = XR::Posef(layer->Quad.QuadPoseCenter);
			quad.size = XR::Vector2f(layer->Quad.QuadSize);
		}

		XrCompositionLayerBaseHeader& header = newLayer.Header;
		header.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		header.space = (session->TrackingSpace == XR_REFERENCE_SPACE_TYPE_STAGE) ?
			session->StageSpace : session->LocalSpace;
	}

	XrFrameEndInfo endInfo = XR_TYPE(FRAME_END_INFO);
	endInfo.displayTime = session->FrameState.predictedDisplayTime;
	//endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
	endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endInfo.layerCount = (uint32_t)layers.size();
	endInfo.layers = layers.data();
	CHK_XR(xrEndFrame(session->Session, &endInfo));

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

	if (strcmp(propertyName, "IPD") == 0)
		return 0.064f;

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
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetFovStencil(
	ovrSession session,
	const ovrFovStencilDesc* fovStencilDesc,
	ovrFovStencilMeshBuffer* meshBuffer)
{
	return ovrError_Unsupported;
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
