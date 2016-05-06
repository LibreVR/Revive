#include "REV_Common.h"

vr::VRTextureBounds_t REV_ViewportToTextureBounds(ovrRecti viewport, ovrTextureSwapChain swapChain, unsigned int flags)
{
	vr::VRTextureBounds_t bounds;
	float w = (float)swapChain->desc.Width;
	float h = (float)swapChain->desc.Height;
	bounds.uMin = viewport.Pos.x / w;
	bounds.vMin = viewport.Pos.y / h;

	// Sanity check for the viewport size.
	// Workaround for Defense Grid 2, which leaves these variables unintialized.
	if (viewport.Size.w > 0 && viewport.Size.h > 0)
	{
		bounds.uMax = (viewport.Pos.x + viewport.Size.w) / w;
		bounds.vMax = (viewport.Pos.y + viewport.Size.h) / h;
	}
	else
	{
		bounds.uMax = 1.0f;
		bounds.vMax = 1.0f;
	}

	if (flags & ovrLayerFlag_TextureOriginAtBottomLeft)
	{
		bounds.vMin = 1.0f - bounds.vMin;
		bounds.vMax = 1.0f - bounds.vMax;
	}

	return bounds;
}

bool REV_IsTouchConnected(vr::TrackedDeviceIndex_t hands[ovrHand_Count])
{
	return hands[ovrHand_Left] != vr::k_unTrackedDeviceIndexInvalid &&
		hands[ovrHand_Right] != vr::k_unTrackedDeviceIndexInvalid &&
		hands[ovrHand_Left] != hands[ovrHand_Right];
}

unsigned int REV_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose)
{
	unsigned int result = 0;

	if (pose.bPoseIsValid)
	{
		if (pose.bDeviceIsConnected)
			result |= ovrStatus_OrientationTracked;
		if (pose.eTrackingResult != vr::TrackingResult_Calibrating_OutOfRange &&
			pose.eTrackingResult != vr::TrackingResult_Running_OutOfRange)
			result |= ovrStatus_PositionTracked;
	}

	return result;
}
