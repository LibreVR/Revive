#pragma once

#include "OVR_CAPI.h"
#include "openvr.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"
#include "TextureBase.h"
#include "Settings.h"

#include <memory>

// Common definitions

#define REV_SWAPCHAIN_LENGTH 2
#define REV_DEFAULT_TIMEOUT 10000

// Common structures

struct ovrTextureSwapChainData
{
	ovrTextureSwapChainDesc Desc;
	vr::ETextureType ApiType;
	vr::VROverlayHandle_t Overlay;

	int Length, CurrentIndex;
	std::unique_ptr<TextureBase> Textures[REV_SWAPCHAIN_LENGTH];
	TextureBase* Submitted;

	ovrTextureSwapChainData(vr::ETextureType api, ovrTextureSwapChainDesc desc);
};

struct ovrMirrorTextureData
{
	ovrMirrorTextureDesc Desc;
	vr::ETextureType ApiType;
	std::unique_ptr<TextureBase> Texture;

	ovrMirrorTextureData(vr::ETextureType api, ovrMirrorTextureDesc desc);
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

	// Revive settings
	float PixelsPerDisplayPixel;
	float Deadzone;
	float Sensitivity;
	revGripType ToggleGrip;
	float ToggleDelay;
	ovrVector3f RotationOffset, PositionOffset;
	vr::HmdMatrix34_t TouchOffset[ovrHand_Count];

	ovrHmdStruct();
	void LoadSettings();
};
