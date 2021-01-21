#define ovr_GetRenderDesc ovr_GetRenderDesc2
#define ovr_SubmitFrame ovr_SubmitFrame2
#include "OVR_CAPI.h"
#undef ovr_GetRenderDesc
#undef ovr_SubmitFrame
#include "Extras/OVR_Math.h"

typedef struct OVR_ALIGNAS(4) ovrEyeRenderDescPre117_ {
	ovrEyeType Eye;
	ovrFovPort Fov;
	ovrRecti DistortedViewport;
	ovrVector2f PixelsPerTanAngleAtCenter;
	ovrVector3f HmdToEyeOffset;
} ovrEyeRenderDescPre117;

OVR_PUBLIC_FUNCTION(ovrEyeRenderDescPre117) ovr_GetRenderDesc(ovrSession session, ovrEyeType eyeType, ovrFovPort fov)
{
	ovrEyeRenderDescPre117 legacy = {};
	ovrEyeRenderDesc desc = ovr_GetRenderDesc2(session, eyeType, fov);
	memcpy(&legacy, &desc, sizeof(ovrEyeRenderDescPre117));
	legacy.HmdToEyeOffset = desc.HmdToEyePose.Position;
	return legacy;
}

typedef struct OVR_ALIGNAS(4) ovrViewScaleDescPre117_ {
	ovrVector3f HmdToEyeOffset[ovrEye_Count]; ///< Translation of each eye.
	float HmdSpaceToWorldScaleInMeters; ///< Ratio of viewer units to meter units.
} ovrViewScaleDescPre117;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame(ovrSession session, long long frameIndex, const ovrViewScaleDescPre117* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	if (viewScaleDesc)
	{
		ovrViewScaleDesc desc = {};
		for (int i = 0; i < ovrEye_Count; i++)
			desc.HmdToEyePose[i] = OVR::Posef(OVR::Quatf(), viewScaleDesc->HmdToEyeOffset[i]);
		desc.HmdSpaceToWorldScaleInMeters = viewScaleDesc->HmdSpaceToWorldScaleInMeters;
		return ovr_SubmitFrame2(session, frameIndex, &desc, layerPtrList, layerCount);
	}
	return ovr_SubmitFrame2(session, frameIndex, nullptr, layerPtrList, layerCount);
}

struct ovrSensorData_;
typedef struct ovrSensorData_ ovrSensorData;

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingStateWithSensorData(ovrSession session, double absTime, ovrBool latencyMarker, ovrSensorData* sensorData)
{
	return ovr_GetTrackingState(session, absTime, latencyMarker);
}

#include "OVR_CAPI_Vk.h"
#undef ovr_SetSynchonizationQueueVk
OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetSynchonizationQueueVk(ovrSession session, VkQueue queue)
{
	return ovr_SetSynchronizationQueueVk(session, queue);
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
