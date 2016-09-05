#include "CompositorGL.h"
#include "REV_Common.h"

#include <GL/glew.h>
#include <Windows.h>

CompositorGL::CompositorGL()
{
}

CompositorGL::~CompositorGL()
{
}

GLenum CompositorGL::TextureFormatToInternalFormat(ovrTextureFormat format)
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
	default: return GL_RGBA8;
	}
}

GLenum CompositorGL::TextureFormatToGLFormat(ovrTextureFormat format)
{
	switch (format)
	{
	case OVR_FORMAT_UNKNOWN:              return GL_RGBA;
	case OVR_FORMAT_B5G6R5_UNORM:         return GL_BGR;
	case OVR_FORMAT_B5G5R5A1_UNORM:       return GL_BGRA;
	case OVR_FORMAT_B4G4R4A4_UNORM:       return GL_BGRA;
	case OVR_FORMAT_R8G8B8A8_UNORM:       return GL_RGBA;
	case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:  return GL_RGBA;
	case OVR_FORMAT_B8G8R8A8_UNORM:       return GL_BGRA;
	case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:  return GL_BGRA;
	case OVR_FORMAT_B8G8R8X8_UNORM:       return GL_BGRA;
	case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return GL_BGRA;
	case OVR_FORMAT_R16G16B16A16_FLOAT:   return GL_RGBA;
	case OVR_FORMAT_D16_UNORM:            return GL_DEPTH_COMPONENT;
	case OVR_FORMAT_D24_UNORM_S8_UINT:    return GL_DEPTH_STENCIL;
	case OVR_FORMAT_D32_FLOAT:            return GL_DEPTH_COMPONENT;
	case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return GL_DEPTH_STENCIL;
	default: return GL_RGBA;
	}
}

ovrResult CompositorGL::CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	GLenum internalFormat = TextureFormatToInternalFormat(desc->Format);
	GLenum format = TextureFormatToGLFormat(desc->Format);

	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData(vr::API_OpenGL, *desc);

	for (int i = 0; i < swapChain->Length; i++)
	{
		swapChain->Textures[i].eType = vr::API_OpenGL;
		swapChain->Textures[i].eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
		glGenTextures(1, (GLuint*)&swapChain->Textures[i].handle);
		glBindTexture(GL_TEXTURE_2D, (GLuint)swapChain->Textures[i].handle);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc->Width, desc->Height, 0, format, GL_UNSIGNED_BYTE, nullptr);
	}

	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

void CompositorGL::DestroyTextureSwapChain(ovrTextureSwapChain chain)
{
	for (int i = 0; i < chain->Length; i++)
		glDeleteTextures(1, (GLuint*)&chain->Textures[i].handle);
}

ovrResult CompositorGL::CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture)
{
	// There can only be one mirror texture at a time
	if (m_MirrorTexture)
		return ovrError_RuntimeException;

	GLenum internalFormat = TextureFormatToInternalFormat(desc->Format);
	GLenum format = TextureFormatToGLFormat(desc->Format);

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData(vr::API_OpenGL, *desc);
	mirrorTexture->Texture.eType = vr::API_OpenGL;
	mirrorTexture->Texture.eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
	glGenTextures(1, (GLuint*)&mirrorTexture->Texture.handle);
	glBindTexture(GL_TEXTURE_2D, (GLuint)mirrorTexture->Texture.handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc->Width, desc->Height, 0, format, GL_UNSIGNED_BYTE, nullptr);

	m_MirrorTexture = mirrorTexture;
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

void CompositorGL::DestroyMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	glDeleteTextures(1, (GLuint*)&mirrorTexture->Texture.handle);
	m_MirrorTexture = nullptr;
}

void CompositorGL::RenderMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	// TODO: Blit the most recently submitted frame to the mirror texture.
}
