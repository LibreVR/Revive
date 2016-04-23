#pragma once

#include "OVR_CAPI.h"

#include "openvr.h"

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
};

struct ovrHmdStruct
{
	// Controller states
	bool ThumbStick[ovrHand_Count];
	bool MenuWasPressed[ovrHand_Count];

	// Mirror window
	ovrTextureSwapChain ColorTexture[ovrEye_Count];

	// Device poses
	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	vr::TrackedDevicePose_t gamePoses[vr::k_unMaxTrackedDeviceCount];

	// OpenVR Interfaces
	vr::IVRCompositor* compositor;
	vr::IVRSettings* settings;
	vr::IVROverlay* overlay;
};
