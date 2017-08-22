#include "CompositorGL.h"
#include "TextureGL.h"
#include "OVR_CAPI.h"

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
	// Get the mirror textures
	glGenFramebuffers(ovrEye_Count, m_mirrorFB);
	for (int i = 0; i < ovrEye_Count; i++)
	{
		vr::VRCompositor()->GetMirrorTextureGL((vr::EVREye)i, &m_mirror[i].first, &m_mirror[i].second);

		glBindFramebuffer(GL_FRAMEBUFFER, m_mirrorFB[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mirror[i].first, 0);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
	}
}

CompositorGL::~CompositorGL()
{
	for (int i = 0; i < ovrEye_Count; i++)
		vr::VRCompositor()->ReleaseSharedGLTexture(m_mirror[i].first, m_mirror[i].second);
}

ovrResult CompositorGL::CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData(vr::TextureType_OpenGL, *desc);
	swapChain->Identifier = m_ChainCount++;

	for (int i = 0; i < swapChain->Length; i++)
	{
		TextureGL* texture = new TextureGL();
		bool success = texture->Create(desc->Width, desc->Height, desc->MipLevels, desc->ArraySize, desc->Format,
			desc->MiscFlags, desc->BindFlags);
		if (!success)
			return ovrError_RuntimeException;
		swapChain->Textures[i].reset(texture);
	}

	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

ovrResult CompositorGL::CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture)
{
	// There can only be one mirror texture at a time
	if (m_MirrorTexture)
		return ovrError_RuntimeException;

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData(vr::TextureType_OpenGL, *desc);
	TextureGL* texture = new TextureGL();
	bool success = texture->Create(desc->Width, desc->Height, 1, 1, desc->Format,
		desc->MiscFlags, 0);
	if (!success)
		return ovrError_RuntimeException;
	mirrorTexture->Texture.reset(texture);

	m_MirrorTexture = mirrorTexture;
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

void CompositorGL::RenderMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	uint32_t width, height;
	vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);

	TextureGL* texture = (TextureGL*)mirrorTexture->Texture.get();
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, texture->Framebuffer);

	for (int i = 0; i < ovrEye_Count; i++)
	{
		vr::VRCompositor()->LockGLSharedTextureForAccess(m_mirror[i].second);

		// Bind the buffer to copy from the compositor to the mirror texture
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_mirrorFB[i]);
		GLint offset = (mirrorTexture->Desc.Width / 2) * i;
		glBlitFramebuffer(0, 0, width, height, offset, 0, offset + mirrorTexture->Desc.Width / 2, mirrorTexture->Desc.Height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		vr::VRCompositor()->UnlockGLSharedTextureForAccess(m_mirror[i].second);
	}
}

void CompositorGL::RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad)
{
	// TODO: Support blending multiple scene layers
}
