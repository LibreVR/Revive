#include "CompositorGL.h"
#include "TextureGL.h"
#include "OVR_CAPI.h"

#include <glad/glad.h>
#include <Windows.h>

GLboolean CompositorGL::gladInitialized = GL_FALSE;

void CompositorGL::DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
}

CompositorGL* CompositorGL::Create()
{
	if (!gladInitialized)
	{
		if (!gladLoadGL())
			return false;
#ifdef DEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback((GLDEBUGPROC)DebugCallback, nullptr);
#else
		glDisable(GL_DEBUG_OUTPUT);
#endif // DEBUG
		glGetError(); // to clear the error caused deep in GLEW
		gladInitialized = GL_TRUE;
	}
	return new CompositorGL();
}

CompositorGL::CompositorGL()
{
	// Get the mirror textures
	/*glGenFramebuffers(ovrEye_Count, m_mirrorFB);
	for (int i = 0; i < ovrEye_Count; i++)
	{
		vr::VRCompositor()->GetMirrorTextureGL((vr::EVREye)i, &m_mirror[i].first, &m_mirror[i].second);

		GLint drawFboId = 0, readFboId = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFboId);

		glBindFramebuffer(GL_FRAMEBUFFER, m_mirrorFB[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mirror[i].first, 0);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFboId);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, readFboId);
	}*/
}

CompositorGL::~CompositorGL()
{
	for (int i = 0; i < ovrEye_Count; i++)
		vr::VRCompositor()->ReleaseSharedGLTexture(m_mirror[i].first, m_mirror[i].second);
}

TextureBase* CompositorGL::CreateTexture()
{
	return new TextureGL();
}

void CompositorGL::RenderMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	/*GLint drawFboId = 0, readFboId = 0, texId = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFboId);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texId);

	TextureGL* texture = (TextureGL*)mirrorTexture->Texture.get();
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, texture->Framebuffer);

	for (int i = 0; i < ovrEye_Count; i++)
	{
		vr::VRCompositor()->LockGLSharedTextureForAccess(m_mirror[i].second);

		GLint width, height;
		glBindTexture(GL_TEXTURE_2D, m_mirror[i].first);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

		// Bind the buffer to copy from the compositor to the mirror texture
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_mirrorFB[i]);
		GLint offset = (mirrorTexture->Desc.Width / 2) * i;
		glBlitFramebuffer(0, 0, width, height, offset, 0, offset + mirrorTexture->Desc.Width / 2, mirrorTexture->Desc.Height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		vr::VRCompositor()->UnlockGLSharedTextureForAccess(m_mirror[i].second);
	}

	glBindTexture(GL_TEXTURE_2D, texId);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFboId);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, readFboId);*/
}

void CompositorGL::RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad)
{
	// TODO: Support blending multiple scene layers
}
