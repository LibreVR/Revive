#include "CompositorGL.h"
#include "OVR_CAPI.h"
#include "REV_Common.h"

#include <GL/glew.h>
#include <Windows.h>

GLboolean CompositorGL::glewInitialized = GL_FALSE;

void CompositorGL::DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
}

CompositorGL* CompositorGL::Create()
{
	if (!glewInitialized)
	{
		glewExperimental = GL_TRUE;
		GLenum nGlewError = glewInit();
		if (nGlewError != GLEW_OK)
			return nullptr;
#ifdef DEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback((GLDEBUGPROC)DebugCallback, nullptr);
#endif // DEBUG
		glGetError(); // to clear the error caused deep in GLEW
		glewInitialized = GL_TRUE;
	}
	return new CompositorGL();
}

CompositorGL::CompositorGL()
{
	// Create the compositor framebuffers.
	uint32_t width, height;
	vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);
	for (int i = 0; i < ovrEye_Count; i++)
	{
		m_CompositorTextures[i].eType = vr::API_OpenGL;
		m_CompositorTextures[i].eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
		GLuint texture = CreateTexture(width, height, OVR_FORMAT_R8G8B8A8_UNORM_SRGB);
		m_CompositorTextures[i].handle = (void*)texture;
		m_CompositorTargets[i] = CreateFramebuffer(texture);
	}
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

GLuint CompositorGL::CreateTexture(GLsizei Width, GLsizei Height, ovrTextureFormat Format)
{
	GLenum internalFormat = TextureFormatToInternalFormat(Format);
	GLenum format = TextureFormatToGLFormat(Format);

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, Width, Height, 0, format, GL_UNSIGNED_BYTE, nullptr);
	return tex;
}

GLuint CompositorGL::CreateFramebuffer(GLuint texture)
{
	GLuint fb;
	glGenFramebuffers(1, &fb);
	glBindFramebuffer(GL_FRAMEBUFFER, fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	return fb;
}

ovrResult CompositorGL::CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData(vr::API_OpenGL, *desc);

	for (int i = 0; i < swapChain->Length; i++)
	{
		swapChain->Textures[i].eType = vr::API_OpenGL;
		swapChain->Textures[i].eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
		GLuint texture = CreateTexture(desc->Width, desc->Height, desc->Format);
		swapChain->Textures[i].handle = (void*)texture;
		swapChain->Views[i] = (void*)CreateFramebuffer(texture);
	}

	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

void CompositorGL::DestroyTextureSwapChain(ovrTextureSwapChain chain)
{
	glDeleteFramebuffers(chain->Length, (GLuint*)chain->Views);
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
	GLuint texture = CreateTexture(desc->Width, desc->Height, desc->Format);
	mirrorTexture->Texture.handle = (void*)texture;
	mirrorTexture->Target = (void*)CreateFramebuffer(texture);

	// TODO: Get the mirror views from OpenVR once they fix the OpenGL implementation.

	m_MirrorTexture = mirrorTexture;
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

void CompositorGL::DestroyMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	glDeleteFramebuffers(1, (GLuint*)&mirrorTexture->Texture.handle);
	glDeleteTextures(1, (GLuint*)&mirrorTexture->Texture.handle);
	m_MirrorTexture = nullptr;
}

void CompositorGL::RenderMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	uint32_t width, height;
	vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);

	for (int i = 0; i < ovrEye_Count; i++)
	{
		// Bind the buffer to copy from the compositor to the mirror texture
		glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)m_CompositorTargets[i]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)mirrorTexture->Target);
		GLint offset = (mirrorTexture->Desc.Width / 2) * i;
		glBlitFramebuffer(0, 0, width, height, offset, mirrorTexture->Desc.Height, offset + mirrorTexture->Desc.Width / 2, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}
}

void CompositorGL::RenderTextureSwapChain(ovrTextureSwapChain chain, vr::EVREye eye, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad)
{
	// TODO: Support blending multiple scene layers
	uint32_t width, height;
	vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);

	// Bind the buffer to copy from the swapchain to the compositor
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)chain->SubmittedView);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_CompositorTargets[eye]);

	// Shrink the bounds to account for the overlapping fov
	float uMin = 0.5f + 0.5f / quad.v[0];
	float uMax = 0.5f + 0.5f / quad.v[1];
	float vMin = 0.5f - 0.5f / quad.v[2];
	float vMax = 0.5f - 0.5f / quad.v[3];

	// Combine the fov bounds with the viewport bounds
	bounds.uMin += uMin * bounds.uMax;
	bounds.uMax *= uMax;
	bounds.vMin += vMin * bounds.vMax;
	bounds.vMax *= vMax;

	// Scale the bounds to the size of the swapchain
	// The vertical bounds are reversed for OpenGL
	GLint srcX0 = (GLint)(bounds.uMin * (float)chain->Desc.Width);
	GLint srcX1 = (GLint)(bounds.uMax * (float)chain->Desc.Width);
	GLint srcY0 = (GLint)(bounds.vMax * (float)chain->Desc.Height);
	GLint srcY1 = (GLint)(bounds.vMin * (float)chain->Desc.Height);

	// Blit the framebuffers
	glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void CompositorGL::ClearScreen()
{
	// Not needed, since we always blit the entire framebuffer
}
