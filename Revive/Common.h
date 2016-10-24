#pragma once

#include "OVR_CAPI.h"
#include "Extras/OVR_Math.h"
#include "openvr.h"
#include "CompositorBase.h"
#include "InputManager.h"
#include "TextureBase.h"

#include <vector>
#include <memory>

// Common structures

#define REV_SWAPCHAIN_LENGTH 2
#define REV_SETTINGS_SECTION "revive"
#define REV_DEFAULT_TIMEOUT 10000

struct ovrTextureSwapChainData
{
	ovrTextureSwapChainDesc Desc;
	vr::EGraphicsAPIConvention ApiType;
	vr::VROverlayHandle_t Overlay;

	int Length, CurrentIndex;
	std::unique_ptr<TextureBase> Textures[REV_SWAPCHAIN_LENGTH];
	TextureBase* Submitted;

	ovrTextureSwapChainData(vr::EGraphicsAPIConvention api, ovrTextureSwapChainDesc desc);
};

struct ovrMirrorTextureData
{
	ovrMirrorTextureDesc Desc;
	vr::EGraphicsAPIConvention ApiType;
	std::unique_ptr<TextureBase> Texture;

	ovrMirrorTextureData(vr::EGraphicsAPIConvention api, ovrMirrorTextureDesc desc);
};

struct ovrHmdStruct
{
	// Session status
	bool Submitted;
	bool ShouldQuit;
	bool IsVisible;
	char StringBuffer[vr::k_unMaxPropertyStringSize];

	// Compositor statistics
	long long FrameIndex;
	long long StatsIndex;
	ovrPerfStatsPerCompositorFrame ResetStats;
	vr::Compositor_CumulativeStats Stats[ovrMaxProvidedFrameStats];

	// Revive interfaces
	CompositorBase* Compositor;
	InputManager* Input;

	ovrHmdStruct();
	~ovrHmdStruct();
};

// Common functions

unsigned int rev_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose);
OVR::Matrix4f rev_HmdMatrixToOVRMatrix(vr::HmdMatrix34_t m);
OVR::Vector3f rev_HmdVectorToOVRVector(vr::HmdVector3_t v);
vr::HmdMatrix34_t rev_OvrPoseToHmdMatrix(ovrPosef pose);
ovrPoseStatef rev_TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, double time);
