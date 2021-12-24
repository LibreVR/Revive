#pragma once

#include "OVR_CAPI.h"
#include "Common.h"

#include <openxr/openxr.h>
#include <guiddef.h>

#define REV_DEFAULT_SWAPCHAIN_DEPTH 3

// {EBA3BA6A-76A2-4A9B-B150-681FC1020EDE}
static const GUID RXR_RTV_DESC =
{ 0xeba3ba6a, 0x76a2, 0x4a9b,{ 0xb1, 0x50, 0x68, 0x1f, 0xc1, 0x2, 0xe, 0xde } };
// {FD3C4A2A-F328-4CC7-9495-A96CF78DEE46}
static const GUID RXR_SRV_DESC =
{ 0xfd3c4a2a, 0xf328, 0x4cc7, { 0x94, 0x95, 0xa9, 0x6c, 0xf7, 0x8d, 0xee, 0x46 } };

struct ovrTextureSwapChainData
{
public:
	ovrTextureSwapChainDesc Desc;
	XrSwapchain Swapchain;
	XrSwapchainImageBaseHeader* Images;
	uint32_t Length;
	uint32_t CurrentIndex;

	virtual ovrResult Commit(ovrSession session);

	ovrResult Init(XrSession session, const ovrTextureSwapChainDesc* desc, int64_t format);

protected:
	template<typename T>
	static ovrResult EnumerateImages(XrStructureType type, ovrTextureSwapChain swapChain)
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

	static bool IsDepthFormat(ovrTextureFormat format);
	static enum DXGI_FORMAT TextureFormatToDXGIFormat(ovrTextureFormat format);
	static enum D3D_SRV_DIMENSION DescToViewDimension(const ovrTextureSwapChainDesc* desc);
	static enum DXGI_FORMAT NegotiateFormat(ovrSession session, enum DXGI_FORMAT format);
};

struct ovrMirrorTextureData
{
	ovrMirrorTextureDesc Desc;
	ovrTextureSwapChain Dummy;
};
