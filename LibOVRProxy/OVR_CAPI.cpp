#include "OVR_CAPI.h"
#include "Extras/OVR_Math.h"

#include <string.h>

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
