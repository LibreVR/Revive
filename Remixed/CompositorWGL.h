#pragma once

#include "CompositorD3D.h"

#include <glad/glad.h>

class CompositorWGL :
	public CompositorD3D
{
public:
	CompositorWGL();
	virtual ~CompositorWGL();

	bool InitInteropDevice();

	virtual TextureBase* CreateTexture();
	virtual void RenderTextureSwapChain(ovrSession session, long long frameIndex, ovrEyeType eye, ovrTextureSwapChain swapChain, ovrRecti viewport);

protected:
	HANDLE m_hInteropDevice;

private:
	static GLboolean gladInitialized;
	static void DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
};
