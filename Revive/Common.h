#pragma once

#include "OVR_CAPI.h"
#include "Extras/OVR_Math.h"
#include "openvr.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"
#include "TextureBase.h"
#include "Settings.h"

#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Common definitions

#define REV_SWAPCHAIN_LENGTH 2
#define REV_DEFAULT_TIMEOUT 10000

// Common structures

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
	bool ShouldQuit;
	bool IsVisible;
	char StringBuffer[vr::k_unMaxPropertyStringSize];

	// Compositor statistics
	long long FrameIndex;
	long long StatsIndex;
	ovrPerfStatsPerCompositorFrame ResetStats;
	vr::Compositor_CumulativeStats Stats[ovrMaxProvidedFrameStats];

	// Revive interfaces
	std::unique_ptr<CompositorBase> Compositor;
	std::unique_ptr<InputManager> Input;
	std::unique_ptr<SessionDetails> Details;

	// Synchronisation
	std::mutex SubmitMutex;

	// Revive settings
	float PixelsPerDisplayPixel;
	vr::HmdMatrix34_t TouchOffset[ovrHand_Count];

	ovrHmdStruct();
};

// Common functions

unsigned int rev_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose);
OVR::Matrix4f rev_HmdMatrixToOVRMatrix(vr::HmdMatrix34_t m);
OVR::Vector3f rev_HmdVectorToOVRVector(vr::HmdVector3_t v);
vr::HmdMatrix34_t rev_OvrPoseToHmdMatrix(ovrPosef pose);
ovrPoseStatef rev_TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, double time);
vr::VRTextureBounds_t rev_FovPortToTextureBounds(ovrEyeType eye, ovrFovPort fov);
void rev_LoadTouchSettings(ovrSession session);
