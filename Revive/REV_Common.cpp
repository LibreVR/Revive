#include "REV_Common.h"

#include <stdio.h>

// Common structures

ovrTextureSwapChainData::ovrTextureSwapChainData(vr::EGraphicsAPIConvention api, ovrTextureSwapChainDesc desc)
	: ApiType(api)
	, Length(2)
	, CurrentIndex(0)
	, Submitted(nullptr)
	, Desc(desc)
	, Overlay(vr::k_ulOverlayHandleInvalid)
{
	memset(Textures, 0, sizeof(Textures));
}

ovrTextureSwapChainData::~ovrTextureSwapChainData()
{
	if (ApiType == vr::API_DirectX)
		rev_DestroyTextureSwapChainDX(this);
	else if (ApiType == vr::API_OpenGL)
		rev_DestroyTextureSwapChainGL(this);
}

ovrMirrorTextureData::ovrMirrorTextureData(vr::EGraphicsAPIConvention api, ovrMirrorTextureDesc desc)
	: ApiType(api)
	, Desc(desc)
	, Target(nullptr)
	, Shader(nullptr)
{
	memset(Views, 0, sizeof(Views));
}

ovrMirrorTextureData::~ovrMirrorTextureData()
{
	if (ApiType == vr::API_DirectX)
		rev_DestroyMirrorTextureDX(this);
	else if (ApiType == vr::API_OpenGL)
		rev_DestroyMirrorTextureGL(this);
}

ovrHmdStruct::ovrHmdStruct()
	: ShouldQuit(false)
	, FrameIndex(0)
	, ThumbStickRange(0.0f)
	, OverlayCount(0)
	, MirrorTexture(nullptr)
{
	memset(ThumbStick, false, sizeof(ThumbStick));
	memset(MenuWasPressed, false, sizeof(MenuWasPressed));

	memset(Poses, 0, sizeof(Poses));
	memset(GamePoses, 0, sizeof(GamePoses));

	// Most games only use the left thumbstick
	ThumbStick[ovrHand_Left] = true;
}

ovrHmdStruct::~ovrHmdStruct()
{
}

// Common functions

vr::VRTextureBounds_t rev_ViewportToTextureBounds(ovrRecti viewport, ovrTextureSwapChain swapChain, unsigned int flags)
{
	vr::VRTextureBounds_t bounds;
	float w = (float)swapChain->Desc.Width;
	float h = (float)swapChain->Desc.Height;
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

bool rev_IsTouchConnected(vr::TrackedDeviceIndex_t hands[ovrHand_Count])
{
	return hands[ovrHand_Left] != vr::k_unTrackedDeviceIndexInvalid &&
		hands[ovrHand_Right] != vr::k_unTrackedDeviceIndexInvalid &&
		hands[ovrHand_Left] != hands[ovrHand_Right];
}

unsigned int rev_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose)
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

vr::VROverlayHandle_t rev_CreateOverlay(ovrSession session)
{
	// Each overlay needs a unique key, so just count how many overlays we've created until now.
	char keyName[vr::k_unVROverlayMaxKeyLength];
	snprintf(keyName, vr::k_unVROverlayMaxKeyLength, "revive.runtime.layer%d", session->OverlayCount++);

	vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
	vr::VROverlay()->CreateOverlay((const char*)keyName, "Revive Layer", &handle);
	return handle;
}
