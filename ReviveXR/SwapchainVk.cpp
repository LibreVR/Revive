#include "SwapchainVk.h"
#include "Common.h"
#include "Session.h"

#define XR_USE_GRAPHICS_API_VULKAN
#include "vulkan.h"
#include <openxr/openxr_platform.h>

VkFormat ovrTextureSwapChainVk::TextureFormatToVkFormat(ovrTextureFormat format)
{
	switch (format)
	{
	case OVR_FORMAT_UNKNOWN:              return VK_FORMAT_UNDEFINED;
	case OVR_FORMAT_B5G6R5_UNORM:         return VK_FORMAT_B5G6R5_UNORM_PACK16;
	case OVR_FORMAT_B5G5R5A1_UNORM:       return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
	case OVR_FORMAT_B4G4R4A4_UNORM:       return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
	case OVR_FORMAT_R8G8B8A8_UNORM:       return VK_FORMAT_R8G8B8A8_UNORM;
	case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:  return VK_FORMAT_R8G8B8A8_SRGB;
	case OVR_FORMAT_B8G8R8A8_UNORM:       return VK_FORMAT_B8G8R8A8_UNORM;
	case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;
	case OVR_FORMAT_B8G8R8X8_UNORM:       return VK_FORMAT_B8G8R8_UNORM;
	case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return VK_FORMAT_B8G8R8_SRGB;
	case OVR_FORMAT_R16G16B16A16_FLOAT:   return VK_FORMAT_R16G16B16A16_SFLOAT;
	case OVR_FORMAT_R11G11B10_FLOAT:      return VK_FORMAT_B10G11R11_UFLOAT_PACK32;

		// Depth formats
	case OVR_FORMAT_D16_UNORM:            return VK_FORMAT_D16_UNORM;
	case OVR_FORMAT_D24_UNORM_S8_UINT:    return VK_FORMAT_D24_UNORM_S8_UINT;
	case OVR_FORMAT_D32_FLOAT:            return VK_FORMAT_D32_SFLOAT;
	case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;

		// Added in 1.5 compressed formats can be used for static layers
	case OVR_FORMAT_BC1_UNORM:            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case OVR_FORMAT_BC1_UNORM_SRGB:       return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
	case OVR_FORMAT_BC2_UNORM:            return VK_FORMAT_BC2_UNORM_BLOCK;
	case OVR_FORMAT_BC2_UNORM_SRGB:       return VK_FORMAT_BC2_SRGB_BLOCK;
	case OVR_FORMAT_BC3_UNORM:            return VK_FORMAT_BC3_UNORM_BLOCK;
	case OVR_FORMAT_BC3_UNORM_SRGB:       return VK_FORMAT_BC3_SRGB_BLOCK;
	case OVR_FORMAT_BC6H_UF16:            return VK_FORMAT_BC6H_UFLOAT_BLOCK;
	case OVR_FORMAT_BC6H_SF16:            return VK_FORMAT_BC6H_SFLOAT_BLOCK;
	case OVR_FORMAT_BC7_UNORM:            return VK_FORMAT_BC7_UNORM_BLOCK;
	case OVR_FORMAT_BC7_UNORM_SRGB:       return VK_FORMAT_BC7_SRGB_BLOCK;

	default:                              return VK_FORMAT_UNDEFINED;
	}
}

ovrResult ovrTextureSwapChainVk::Create(ovrSession session, const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	ovrTextureSwapChain chain = new ovrTextureSwapChainVk();
	CHK_OVR(chain->Init(session->Session, desc, TextureFormatToVkFormat(desc->Format)));
	CHK_OVR(EnumerateImages<XrSwapchainImageVulkanKHR>(XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR, chain));
	*out_TextureSwapChain = chain;
	return ovrSuccess;
}
