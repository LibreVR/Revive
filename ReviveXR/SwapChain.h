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

template<typename T>
ovrResult EnumerateImages(XrStructureType type, ovrTextureSwapChain swapChain)
{
	CHK_XR(xrEnumerateSwapchainImages(swapChain->Swapchain, 0, &swapChain->Length, nullptr));
	T* images = new T[swapChain->Length]();
	for (uint32_t i = 0; i < swapChain->Length; i++)
		images[i].type = type;
	swapChain->Images = (XrSwapchainImageBaseHeader*)images;

	uint32_t finalLength;
	CHK_XR(xrEnumerateSwapchainImages(swapChain->Swapchain, swapChain->Length, &finalLength, swapChain->Images));
	assert(swapChain->Length == finalLength);
	return ovrSuccess;
}

ovrResult CreateSwapChain(XrSession session, const ovrTextureSwapChainDesc* desc, int64_t format, ovrTextureSwapChain* out);
