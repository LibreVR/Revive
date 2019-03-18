#include "SwapChain.h"
#include "Common.h"

#include <openxr/openxr.h>
#include <assert.h>

XrSwapchainCreateInfo DescToCreateInfo(const ovrTextureSwapChainDesc* desc, uint32_t format)
{
	XrSwapchainCreateInfo createInfo = XR_TYPE(SWAPCHAIN_CREATE_INFO);

	// TODO: Support typeless flag
	//assert(!(desc->MiscFlags & ovrTextureMisc_DX_Typeless));

	if (desc->MiscFlags & ovrTextureMisc_ProtectedContent)
		createInfo.createFlags |= XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT;

	if (desc->StaticImage)
		createInfo.createFlags |= XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;

	createInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

	if (desc->BindFlags & ovrTextureBind_DX_RenderTarget)
		createInfo.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

	if (desc->BindFlags & ovrTextureBind_DX_UnorderedAccess)
		createInfo.usageFlags |= XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

	if (desc->BindFlags & ovrTextureBind_DX_DepthStencil)
		createInfo.usageFlags |= XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	createInfo.format = format;
	createInfo.sampleCount = desc->SampleCount;
	createInfo.width = desc->Width;
	createInfo.height = desc->Height;
	createInfo.faceCount = desc->ArraySize; // This is actually face count
	createInfo.arraySize = 1;
	createInfo.mipCount = desc->MipLevels;
	return createInfo;
}
