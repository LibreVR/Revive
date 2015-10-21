#pragma once

#include "OVR_CAPI.h"

#include "openvr.h"

struct ovrTextureSwapChainData {
	int length, index;
	ovrTextureSwapChainDesc desc;
	vr::Texture_t texture;
};

struct ovrHmdStruct
{
	vr::IVRCompositor* compositor;
	vr::IVRSettings* settings;
};
