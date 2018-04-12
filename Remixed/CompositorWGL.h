#pragma once

#include "CompositorD3D.h"

#include <glad/glad.h>

#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

class CompositorWGL :
	public CompositorD3D
{
public:
	CompositorWGL();
	virtual ~CompositorWGL();

	bool InitInteropDevice();

	virtual TextureBase* CreateTexture();
	virtual void RenderTextureSwapChain(winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface surface,
		ovrTextureSwapChain swapChain, ovrRecti viewport, ovrEyeType eye);

protected:
	HANDLE m_hInteropDevice;

private:
	static GLboolean gladInitialized;
	static void DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
};
