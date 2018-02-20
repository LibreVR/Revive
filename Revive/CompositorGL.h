#pragma once

#include "CompositorBase.h"

#include <glad/glad.h>
#include <openvr.h>
#include <utility>

class CompositorGL :
	public CompositorBase
{
public:
	CompositorGL();
	virtual ~CompositorGL();

	static CompositorGL* Create();
	virtual vr::ETextureType GetAPI() { return vr::TextureType_OpenGL; };
	virtual void Flush() { glFlush(); };
	virtual TextureBase* CreateTexture();

	virtual void RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture);

protected:
	std::pair<vr::glUInt_t, vr::glSharedTextureHandle_t> m_mirror[ovrEye_Count];
	GLuint m_mirrorFB[ovrEye_Count];

private:
	static GLboolean gladInitialized;
	static void DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
};
