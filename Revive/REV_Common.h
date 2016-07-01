#pragma once

#include "OVR_CAPI.h"
#include "openvr.h"
#include <vector>

// Common structures

struct ovrTextureSwapChainData
{
	ovrTextureSwapChainDesc Desc;
	vr::EGraphicsAPIConvention ApiType;
	int Length, CurrentIndex;
	vr::Texture_t Textures[2];
	vr::VROverlayHandle_t Overlay;

	ovrTextureSwapChainData(vr::EGraphicsAPIConvention api, ovrTextureSwapChainDesc desc);
	~ovrTextureSwapChainData();

	vr::Texture_t* Current() { return &Textures[CurrentIndex]; }
};

struct ovrMirrorTextureData
{
	ovrMirrorTextureDesc Desc;
	vr::EGraphicsAPIConvention ApiType;
	vr::Texture_t Texture;
	void* Views[ovrEye_Count];
	void* Target;
	void* Shader;

	ovrMirrorTextureData(vr::EGraphicsAPIConvention api, ovrMirrorTextureDesc desc);
	~ovrMirrorTextureData();
};

struct ovrHmdStruct
{
	// Session status
	bool ShouldQuit;

	// Controller states
	bool ThumbStick[ovrHand_Count];
	bool MenuWasPressed[ovrHand_Count];
	float ThumbStickRange;

	// Device poses
	vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];
	vr::TrackedDevicePose_t GamePoses[vr::k_unMaxTrackedDeviceCount];

	// Overlays
	unsigned int OverlayCount;
	std::vector<vr::VROverlayHandle_t> ActiveOverlays;

	ovrHmdStruct();
	~ovrHmdStruct();
};

// Common functions

bool REV_IsTouchConnected(vr::TrackedDeviceIndex_t hands[ovrHand_Count]);
unsigned int REV_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose);
vr::VRTextureBounds_t REV_ViewportToTextureBounds(ovrRecti viewport, ovrTextureSwapChain swapChain, unsigned int flags);
vr::VROverlayHandle_t REV_CreateOverlay(ovrSession session);

// Graphics functions

void ovr_DestroyTextureSwapChainDX(ovrTextureSwapChain chain);
void ovr_DestroyTextureSwapChainGL(ovrTextureSwapChain chain);
void ovr_DestroyMirrorTextureDX(ovrMirrorTexture mirrorTexture);
void ovr_DestroyMirrorTextureGL(ovrMirrorTexture mirrorTexture);
