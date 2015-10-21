#include "Extras/OVR_CAPI_Util.h"

#include "openvr.h"

#include "REV_Assert.h"

OVR_PUBLIC_FUNCTION(ovrDetectResult) ovr_Detect(int timeoutMilliseconds)
{
	ovrDetectResult result;
	result.IsOculusHMDConnected = vr::VR_IsHmdPresent();
	result.IsOculusServiceRunning = vr::VR_IsRuntimeInstalled();
	return result;
}

OVR_PUBLIC_FUNCTION(ovrMatrix4f) ovrMatrix4f_Projection(ovrFovPort fov, float znear, float zfar, unsigned int projectionModFlags) { REV_UNIMPLEMENTED_STRUCT(ovrMatrix4f); }

OVR_PUBLIC_FUNCTION(ovrTimewarpProjectionDesc) ovrTimewarpProjectionDesc_FromProjection(ovrMatrix4f projection, unsigned int projectionModFlags) { REV_UNIMPLEMENTED_STRUCT(ovrTimewarpProjectionDesc); }

OVR_PUBLIC_FUNCTION(ovrMatrix4f) ovrMatrix4f_OrthoSubProjection(ovrMatrix4f projection, ovrVector2f orthoScale,
                                                                float orthoDistance, float HmdToEyeOffsetX) { REV_UNIMPLEMENTED_STRUCT(ovrMatrix4f); }

OVR_PUBLIC_FUNCTION(void) ovr_CalcEyePoses(ovrPosef headPose,
                                           const ovrVector3f HmdToEyeOffset[2],
                                           ovrPosef outEyePoses[2]) { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(void) ovr_GetEyePoses(ovrSession session, long long frameIndex, ovrBool latencyMarker,
                                             const ovrVector3f HmdToEyeOffset[2],
                                             ovrPosef outEyePoses[2],
                                             double* outSensorSampleTime) { REV_UNIMPLEMENTED; }

OVR_PUBLIC_FUNCTION(void) ovrPosef_FlipHandedness(const ovrPosef* inPose, ovrPosef* outPose) { REV_UNIMPLEMENTED; }
