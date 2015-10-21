#include "OVR_CAPI.h"
#include "openvr.h"

#include "REV_Assert.h"
#include "REV_Error.h"

vr::IVRSystem* g_VRSystem;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params)
{
	vr::EVRInitError err;
	g_VRSystem = vr::VR_Init(&err, vr::VRApplication_Scene);
	return EVR_InitErrorToOvrError(err);
}

OVR_PUBLIC_FUNCTION(void) ovr_Shutdown() { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo) { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString() { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(int) ovr_TraceMessage(int level, const char* message) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrHmdDesc) ovr_GetHmdDesc(ovrSession session) { REV_UNIMPLEMENTED_STRUCT(ovrHmdDesc); }

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackerCount(ovrSession session) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrTrackerDesc) ovr_GetTrackerDesc(ovrSession session, unsigned int trackerDescIndex) { REV_UNIMPLEMENTED_STRUCT(ovrTrackerDesc); }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Create(ovrSession* pSession, ovrGraphicsLuid* pLuid) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(void) ovr_Destroy(ovrSession session) { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetSessionStatus(ovrSession session, ovrSessionStatus* sessionStatus) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetTrackingOriginType(ovrSession session, ovrTrackingOrigin origin) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrTrackingOrigin) ovr_GetTrackingOriginType(ovrSession session) { REV_UNIMPLEMENTED_STRUCT(ovrTrackingOrigin); }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RecenterTrackingOrigin(ovrSession session) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(void) ovr_ClearShouldRecenterFlag(ovrSession session) { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingState(ovrSession session, double absTime, ovrBool latencyMarker) { REV_UNIMPLEMENTED_STRUCT(ovrTrackingState); }

OVR_PUBLIC_FUNCTION(ovrTrackerPose) ovr_GetTrackerPose(ovrSession session, unsigned int trackerPoseIndex) { REV_UNIMPLEMENTED_STRUCT(ovrTrackerPose); }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetConnectedControllerTypes(ovrSession session) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainLength(ovrSession session, ovrTextureSwapChain chain, int* out_Length) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainCurrentIndex(ovrSession session, ovrTextureSwapChain chain, int* out_Index) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainDesc(ovrSession session, ovrTextureSwapChain chain, ovrTextureSwapChainDesc* out_Desc) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CommitTextureSwapChain(ovrSession session, ovrTextureSwapChain chain) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(void) ovr_DestroyTextureSwapChain(ovrSession session, ovrTextureSwapChain chain) { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(void) ovr_DestroyMirrorTexture(ovrSession session, ovrMirrorTexture mirrorTexture) { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(ovrSizei) ovr_GetFovTextureSize(ovrSession session, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel) { REV_UNIMPLEMENTED_STRUCT(ovrSizei); }

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovr_GetRenderDesc(ovrSession session, ovrEyeType eyeType, ovrFovPort fov) { REV_UNIMPLEMENTED_STRUCT(ovrEyeRenderDesc); }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(double) ovr_GetPredictedDisplayTime(ovrSession session, long long frameIndex) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds() { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_GetBool(ovrSession session, const char* propertyName, ovrBool defaultVal) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetBool(ovrSession session, const char* propertyName, ovrBool value) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(int) ovr_GetInt(ovrSession session, const char* propertyName, int defaultVal) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetInt(ovrSession session, const char* propertyName, int value) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(float) ovr_GetFloat(ovrSession session, const char* propertyName, float defaultVal) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloat(ovrSession session, const char* propertyName, float value) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetFloatArray(ovrSession session, const char* propertyName, float values[], unsigned int valuesCapacity) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloatArray(ovrSession session, const char* propertyName, const float values[], unsigned int valuesSize) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(const char*) ovr_GetString(ovrSession session, const char* propertyName, const char* defaultVal) { REV_UNIMPLEMENTED_NULL; }

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetString(ovrSession session, const char* propertyName, const char* value) { REV_UNIMPLEMENTED_NULL; }
