#include "SwapChain.h"
#include "Common.h"

#include <openxr/openxr.h>
#include <vector>

XrSwapchainCreateInfo DescToCreateInfo(const ovrTextureSwapChainDesc* desc, int64_t format)
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

ovrResult CreateSwapChain(XrSession session, const ovrTextureSwapChainDesc* desc, int64_t format, ovrTextureSwapChain* out)
{
	// Enumerate formats
	uint32_t formatCount = 0;
	xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr);
	std::vector<int64_t> formats;
	formats.resize(formatCount);
	xrEnumerateSwapchainFormats(session, (uint32_t)formats.size(), &formatCount, formats.data());
	assert(formats.size() == formatCount);

	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData();

	// Check if the format is supported
	if (std::find(formats.begin(), formats.end(), format) == formats.end()) {
		return ovrError_InvalidParameter;
	}

	swapChain->Desc = *desc;
	XrSwapchainCreateInfo createInfo = DescToCreateInfo(desc, format);
	CHK_XR(xrCreateSwapchain(session, &createInfo, &swapChain->Swapchain));

	XrSwapchainImageAcquireInfo acqInfo = XR_TYPE(SWAPCHAIN_IMAGE_ACQUIRE_INFO);
	CHK_XR(xrAcquireSwapchainImage(swapChain->Swapchain, &acqInfo, &swapChain->CurrentIndex));

	XrSwapchainImageWaitInfo waitInfo = XR_TYPE(SWAPCHAIN_IMAGE_WAIT_INFO);
	waitInfo.timeout = XR_NO_DURATION;
	CHK_XR(xrWaitSwapchainImage(swapChain->Swapchain, &waitInfo));

	*out = swapChain;
	return ovrSuccess;
}
