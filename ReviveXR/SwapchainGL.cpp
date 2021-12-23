#include "SwapchainGL.h"
#include "Common.h"
#include "Session.h"

#include <Windows.h>
#define XR_USE_GRAPHICS_API_OPENGL
#include <glad/glad.h>
#include <openxr/openxr_platform.h>

unsigned int ovrTextureSwapChainGL::TextureFormatToGLFormat(ovrTextureFormat format)
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

ovrResult ovrTextureSwapChainGL::Create(ovrSession session, const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	ovrTextureSwapChain chain = new ovrTextureSwapChainGL();
	CHK_OVR(chain->Init(session->Session, desc, TextureFormatToGLFormat(desc->Format)));
	CHK_OVR(EnumerateImages<XrSwapchainImageOpenGLKHR>(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, chain));
	*out_TextureSwapChain = chain;
	return ovrSuccess;
}
