#pragma once

#include "OVR_CAPI.h"
#include <openxr/openxr.h>

#define REV_DEFAULT_SWAPCHAIN_DEPTH 3

struct ovrTextureSwapChainData
{
	ovrTextureSwapChainDesc Desc;
	XrSwapchain Swapchain;
	XrSwapchainImageBaseHeader* Images;
	uint32_t Length;
	uint32_t CurrentIndex;
};

struct ovrMirrorTextureData
{
	ovrMirrorTextureDesc Desc;
	ovrTextureSwapChain Dummy;
};

XrSwapchainCreateInfo DescToCreateInfo(const ovrTextureSwapChainDesc* desc, int64_t format);
