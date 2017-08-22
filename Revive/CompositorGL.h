#pragma once

#include "CompositorBase.h"

#include <GL/glew.h>
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

	// Texture Swapchain
	virtual ovrResult CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);
	virtual void RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad);

	// Mirror Texture
	virtual ovrResult CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture);

protected:
	static GLboolean glewInitialized;

	std::pair<vr::glUInt_t, vr::glSharedTextureHandle_t> m_mirror[ovrEye_Count];
	GLuint m_mirrorFB[ovrEye_Count];

private:
	static void DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
};
