#include "OVR_CAPI_GL.h"
#include "Common.h"
#include "Session.h"
#include "SwapChain.h"

#include <vector>

#include <Windows.h>
#define XR_USE_GRAPHICS_API_OPENGL
#include <glad/glad.h>
#include <openxr/openxr_platform.h>

unsigned int TextureFormatToGLFormat(ovrTextureFormat format)
{
	switch (format)
	{
		case OVR_FORMAT_UNKNOWN:              return GL_RGBA8;
		case OVR_FORMAT_B5G6R5_UNORM:         return GL_RGB565;
		case OVR_FORMAT_B5G5R5A1_UNORM:       return GL_RGB5_A1;
		case OVR_FORMAT_B4G4R4A4_UNORM:       return GL_RGBA4;
		case OVR_FORMAT_R8G8B8A8_UNORM:       return GL_RGBA8;
		case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:  return GL_SRGB8_ALPHA8;
		case OVR_FORMAT_B8G8R8A8_UNORM:       return GL_RGBA8;
		case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:  return GL_SRGB8_ALPHA8;
		case OVR_FORMAT_B8G8R8X8_UNORM:       return GL_RGBA8;
		case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return GL_SRGB8_ALPHA8;
		case OVR_FORMAT_R16G16B16A16_FLOAT:   return GL_RGBA16F;
		case OVR_FORMAT_D16_UNORM:            return GL_DEPTH_COMPONENT16;
		case OVR_FORMAT_D24_UNORM_S8_UINT:    return GL_DEPTH24_STENCIL8;
		case OVR_FORMAT_D32_FLOAT:            return GL_DEPTH_COMPONENT32F;
		case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return GL_DEPTH32F_STENCIL8;
		default:                              return GL_RGBA8;
	}
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainGL(ovrSession session,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	REV_TRACE(ovr_CreateTextureSwapChainGL);

	if (!session)
		return ovrError_InvalidSession;

	if (!desc || !out_TextureSwapChain || desc->Type != ovrTexture_2D)
		return ovrError_InvalidParameter;

	if (!session->Session)
	{
		XrGraphicsBindingOpenGLWin32KHR graphicsBinding = XR_TYPE(GRAPHICS_BINDING_OPENGL_WIN32_KHR);
		graphicsBinding.hDC = wglGetCurrentDC();
		graphicsBinding.hGLRC = wglGetCurrentContext();
		session->BeginSession(&graphicsBinding);
	}

	// Enumerate formats
	uint32_t formatCount = 0;
	xrEnumerateSwapchainFormats(session->Session, 0, &formatCount, nullptr);
	std::vector<int64_t> formats;
	formats.resize(formatCount);
	xrEnumerateSwapchainFormats(session->Session, (uint32_t)formats.size(), &formatCount, formats.data());
	assert(formats.size() == formatCount);

	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData();

	// Check if the format is supported
	swapChain->Format = TextureFormatToGLFormat(desc->Format);

	if (std::find(formats.begin(), formats.end(), swapChain->Format) == formats.end()) {
		swapChain->Format = formats[0];
	}

	swapChain->Desc = *desc;
	XrSwapchainCreateInfo createInfo = DescToCreateInfo(desc, swapChain->Format);
	CHK_XR(xrCreateSwapchain(session->Session, &createInfo, &swapChain->Swapchain));

	CHK_XR(xrEnumerateSwapchainImages(swapChain->Swapchain, 0, &swapChain->Length, nullptr));
	XrSwapchainImageOpenGLKHR* images = new XrSwapchainImageOpenGLKHR[swapChain->Length];
	for (uint32_t i = 0; i < swapChain->Length; i++)
		images[i] = XR_TYPE(SWAPCHAIN_IMAGE_D3D11_KHR);
	swapChain->Images = (XrSwapchainImageBaseHeader*)images;

	uint32_t finalLength;
	CHK_XR(xrEnumerateSwapchainImages(swapChain->Swapchain, swapChain->Length, &finalLength, swapChain->Images));
	assert(swapChain->Length == finalLength);

	XrSwapchainImageAcquireInfo acqInfo = XR_TYPE(SWAPCHAIN_IMAGE_ACQUIRE_INFO);
	CHK_XR(xrAcquireSwapchainImage(swapChain->Swapchain, &acqInfo, &swapChain->CurrentIndex));

	XrSwapchainImageWaitInfo waitInfo = XR_TYPE(SWAPCHAIN_IMAGE_WAIT_INFO);
	waitInfo.timeout = XR_NO_DURATION;
	CHK_XR(xrWaitSwapchainImage(swapChain->Swapchain, &waitInfo));

	*out_TextureSwapChain = swapChain;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainBufferGL(ovrSession session,
                                                               ovrTextureSwapChain chain,
                                                               int index,
                                                               unsigned int* out_TexId)
{
	REV_TRACE(ovr_GetTextureSwapChainBufferGL);

	if (!session)
		return ovrError_InvalidSession;

	if (!chain || !out_TexId)
		return ovrError_InvalidParameter;

	if (index < 0)
		index = chain->CurrentIndex;

	*out_TexId = ((XrSwapchainImageOpenGLKHR*)chain->Images)[index].image;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureGL(ovrSession session,
                                                         const ovrMirrorTextureDesc* desc,
                                                         ovrMirrorTexture* out_MirrorTexture)
{
	REV_TRACE(ovr_CreateMirrorTextureGL);

	if (!session)
		return ovrError_InvalidSession;

	if (!desc || !out_MirrorTexture)
		return ovrError_InvalidParameter;

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData();
	mirrorTexture->Desc = *desc;
	*out_MirrorTexture = mirrorTexture;

	ovrTextureSwapChainDesc swapDesc;
	swapDesc.Type = ovrTexture_2D;
	swapDesc.Format = desc->Format;
	swapDesc.ArraySize = 1;
	swapDesc.Width = desc->Width;
	swapDesc.Height = desc->Height;
	swapDesc.MipLevels = 1;
	swapDesc.SampleCount = 1;
	swapDesc.StaticImage = ovrTrue;
	swapDesc.MiscFlags = desc->MiscFlags;
	swapDesc.BindFlags = 0;
	return ovr_CreateTextureSwapChainGL(session, &swapDesc, &mirrorTexture->Dummy);
}


OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureWithOptionsGL(ovrSession session,
                                                                    const ovrMirrorTextureDesc* desc,
                                                                    ovrMirrorTexture* out_MirrorTexture)
{
	return ovr_CreateMirrorTextureGL(session, desc, out_MirrorTexture);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetMirrorTextureBufferGL(ovrSession session,
                                                            ovrMirrorTexture mirrorTexture,
                                                            unsigned int* out_TexId)
{
	REV_TRACE(ovr_GetMirrorTextureBufferGL);

	if (!session)
		return ovrError_InvalidSession;

	if (!mirrorTexture || !out_TexId)
		return ovrError_InvalidParameter;

	return ovr_GetTextureSwapChainBufferGL(session, mirrorTexture->Dummy, 0, out_TexId);
}
