#include "TextureGL.h"

#include <Windows.h>
#include <glad/glad.h>
#include <glad/glad_wgl.h>

TextureGL::TextureGL()
	: Texture(0)
	, Framebuffer(0)
	, ResolveTexture(0)
	, ResolveFramebuffer(0)
	, m_Width(0)
	, m_Height(0)
	, m_InteropTexture(0)
{
}

TextureGL::~TextureGL()
{
	glDeleteFramebuffers(1, &Framebuffer);
	glDeleteTextures(1, &Texture);
}

void TextureGL::ToVRTexture(vr::Texture_t& texture)
{
	if (ResolveTexture)
	{
		glBlitNamedFramebuffer(Framebuffer, ResolveFramebuffer,
			0, 0, m_Width, m_Height, 0, 0, m_Width, m_Height,
			GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	texture.eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format
	texture.eType = vr::TextureType_OpenGL;
#pragma warning( disable : 4312 )
	texture.handle = (void*)static_cast<uintptr_t>(m_InteropTexture ? m_InteropTexture :
		ResolveTexture ? ResolveTexture : Texture);
#pragma warning( default : 4312 )
}

unsigned int TextureGL::TextureFormatToInternalFormat(ovrTextureFormat format)
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

unsigned int TextureGL::TextureFormatToGLFormat(ovrTextureFormat format)
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

bool TextureGL::Init(ovrTextureType Type, int Width, int Height, int MipLevels, int SampleCount,
	int ArraySize, ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags)
{
	GLenum internalFormat = TextureFormatToInternalFormat(Format);
	GLenum format = TextureFormatToGLFormat(Format);

	glCreateTextures(SampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &Texture);
	glTextureParameteri(Texture, GL_TEXTURE_BASE_LEVEL, 0);
	glTextureParameteri(Texture, GL_TEXTURE_MAX_LEVEL, 0);
	if (SampleCount > 1)
		glTextureStorage2DMultisample(Texture, SampleCount, internalFormat, Width, Height, GL_FALSE);
	else
		glTextureStorage2D(Texture, 1, internalFormat, Width, Height);

	m_Width = Width;
	m_Height = Height;

	if (SampleCount > 1 || Type == ovrTexture_2D_External)
	{
		GLenum attachment = format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL ?
			GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;

		glCreateFramebuffers(1, &Framebuffer);
		glNamedFramebufferTexture(Framebuffer, attachment, Texture, 0);
		glNamedFramebufferReadBuffer(Framebuffer, attachment);
		glNamedFramebufferDrawBuffer(Framebuffer, attachment);
		GLenum completeness = glCheckNamedFramebufferStatus(Framebuffer, GL_FRAMEBUFFER);
		if (completeness != GL_FRAMEBUFFER_COMPLETE)
			return false;

		if (SampleCount > 1)
		{
			// SteamVR doesn't support OpenGL MSAA textures, so we have to resolve it before returning a VR texture
			glCreateTextures(GL_TEXTURE_2D, 1, &ResolveTexture);
			glTextureParameteri(ResolveTexture, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(ResolveTexture, GL_TEXTURE_MAX_LEVEL, 0);
			glTextureStorage2D(ResolveTexture, 1, internalFormat, Width, Height);

			glCreateFramebuffers(1, &ResolveFramebuffer);
			glNamedFramebufferTexture(ResolveFramebuffer, attachment, ResolveTexture, 0);
			glNamedFramebufferReadBuffer(ResolveFramebuffer, attachment);
			glNamedFramebufferDrawBuffer(ResolveFramebuffer, attachment);
			if (glCheckNamedFramebufferStatus(ResolveFramebuffer, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				return false;
		}
	}

	return true;
}

bool TextureGL::CreateSharedTextureGL(unsigned int* outName)
{
	// We don't actually share a texture, we just create a new one in the current context
	glCreateTextures(GL_TEXTURE_2D, 1, &m_InteropTexture);
	glTextureParameteri(m_InteropTexture, GL_TEXTURE_BASE_LEVEL, 0);
	glTextureParameteri(m_InteropTexture, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureStorage2D(m_InteropTexture, 1, GL_RGBA8, m_Width, m_Height);
	*outName = m_InteropTexture;
	return true;
}

void TextureGL::DeleteSharedTextureGL(unsigned int name)
{
	glDeleteTextures(1, &name);
}
