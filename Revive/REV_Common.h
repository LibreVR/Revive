#pragma once

#include "OVR_CAPI.h"
#include "openvr.h"
#include "CompositorBase.h"
#include "InputManager.h"

#include <vector>

// Common structures

#define REV_SWAPCHAIN_LENGTH 2
#define REV_SETTINGS_SECTION "revive"

struct ovrTextureSwapChainData
{
	ovrTextureSwapChainDesc Desc;
	vr::EGraphicsAPIConvention ApiType;

	int Length, CurrentIndex;
	vr::Texture_t Textures[REV_SWAPCHAIN_LENGTH];
	void* Views[REV_SWAPCHAIN_LENGTH];

	vr::Texture_t* SubmittedTexture;
	void* SubmittedView;
	vr::VROverlayHandle_t Overlay;

	ovrTextureSwapChainData(vr::EGraphicsAPIConvention api, ovrTextureSwapChainDesc desc);
};

struct ovrMirrorTextureData
{
	ovrMirrorTextureDesc Desc;
	vr::EGraphicsAPIConvention ApiType;
	vr::Texture_t Texture;
	void* Views[ovrEye_Count];
	void* Target;

	ovrMirrorTextureData(vr::EGraphicsAPIConvention api, ovrMirrorTextureDesc desc);
};

struct ovrHmdStruct
{
	// Session status
	bool ShouldQuit;
	bool IsVisible;
	char StringBuffer[vr::k_unMaxPropertyStringSize];
	long long FrameIndex;

	// Revive interfaces
	CompositorBase* Compositor;
	InputManager* Input;

	ovrHmdStruct();
	~ovrHmdStruct();
};

// Common functions

unsigned int rev_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose);
