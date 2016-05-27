#pragma once

#include "OVR_CAPI.h"

#include "openvr.h"

// Common structures

struct ovrTextureSwapChainData
{
	int length, index;
	ovrTextureSwapChainDesc desc;
	vr::Texture_t current;
	vr::Texture_t texture[2];
};

struct ovrMirrorTextureData
{
	ovrMirrorTextureDesc desc;
	vr::Texture_t texture;
	void* target;
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
	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	vr::TrackedDevicePose_t gamePoses[vr::k_unMaxTrackedDeviceCount];

	// OpenVR interfaces
	vr::IVRCompositor* compositor;
	vr::IVRSettings* settings;
	vr::IVROverlay* overlay;
};

// Common functions

bool REV_IsTouchConnected(vr::TrackedDeviceIndex_t hands[ovrHand_Count]);
unsigned int REV_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose);
vr::VRTextureBounds_t REV_ViewportToTextureBounds(ovrRecti viewport, ovrTextureSwapChain swapChain, unsigned int flags);
