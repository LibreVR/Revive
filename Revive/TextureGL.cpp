#include "TextureGL.h"

#include <GL/glew.h>

TextureGL::TextureGL()
	: Texture(0)
	, Framebuffer(0)
{
}

TextureGL::~TextureGL()
{
	glDeleteFramebuffers(1, &Framebuffer);
	glDeleteTextures(1, &Texture);
}

vr::Texture_t TextureGL::ToVRTexture()
{
	vr::Texture_t texture;
	texture.eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format
	texture.eType = vr::API_OpenGL;
#pragma warning( disable : 4312 )
	texture.handle = (void*)Texture;
#pragma warning( default : 4312 )
	return texture;
}

GLenum TextureGL::TextureFormatToInternalFormat(ovrTextureFormat format)
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

GLenum TextureGL::TextureFormatToGLFormat(ovrTextureFormat format)
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
		default:                              return GL_RGBA;
	}
}

bool TextureGL::Create(int Width, int Height, int MipLevels, int ArraySize,
	ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags)
{
	GLenum internalFormat = TextureFormatToInternalFormat(Format);
	GLenum format = TextureFormatToGLFormat(Format);

	glGenTextures(1, &Texture);
	glBindTexture(GL_TEXTURE_2D, Texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, Width, Height, 0, format, GL_UNSIGNED_BYTE, nullptr);

	glGenFramebuffers(1, &Framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Texture, 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}
