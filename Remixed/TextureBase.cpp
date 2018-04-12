#include "TextureBase.h"

#include <winrt/Windows.Foundation.h>
using namespace winrt::Windows::Foundation;

#include <winrt/Windows.Graphics.DirectX.h>
using namespace winrt::Windows::Graphics::DirectX;

#include <winrt/Windows.Graphics.Holographic.h>
using namespace winrt::Windows::Graphics::Holographic;

bool TextureBase::IsSRGBFormat(ovrTextureFormat format)
{
	switch (format)
	{
		case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:  return true;
		case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:  return true;
		case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return true;

		// Added in 1.5 compressed formats can be used for static layers
		case OVR_FORMAT_BC1_UNORM_SRGB:       return true;
		case OVR_FORMAT_BC2_UNORM_SRGB:       return true;
		case OVR_FORMAT_BC3_UNORM_SRGB:       return true;
		case OVR_FORMAT_BC7_UNORM_SRGB:       return true;

		default: return false;
	}
}

bool TextureBase::IsDepthFormat(ovrTextureFormat format)
{
	switch (format)
	{
		case OVR_FORMAT_D16_UNORM:            return true;
		case OVR_FORMAT_D24_UNORM_S8_UINT:    return true;
		case OVR_FORMAT_D32_FLOAT:            return true;
		case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return true;

		default: return false;
	}
}

ovrTextureSwapChainData::ovrTextureSwapChainData(ovrTextureSwapChainDesc desc)
	: Desc(desc)
	, Overlay(nullptr)
	, Length(REV_SWAPCHAIN_MAX_LENGTH)
	, Identifier(0)
	, CurrentIndex(0)
	, SubmitIndex(0)
	, Textures()
{
}

ovrTextureSwapChainData::~ovrTextureSwapChainData()
{
}

ovrMirrorTextureData::ovrMirrorTextureData(ovrMirrorTextureDesc desc)
	: Desc(desc)
	, Texture()
{
}

ovrMirrorTextureData::~ovrMirrorTextureData()
{
}
