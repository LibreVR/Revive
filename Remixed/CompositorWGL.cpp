#include "CompositorWGL.h"
#include "TextureWGL.h"

#include <glad/glad.h>
#include <glad/glad_wgl.h>

#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

GLboolean CompositorWGL::gladInitialized = GL_FALSE;

void CompositorWGL::DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
		return;

	OutputDebugStringA(message);
	OutputDebugStringA("\n");
}

CompositorWGL::CompositorWGL()
	: m_hInteropDevice()
{
}

CompositorWGL::~CompositorWGL()
{
	if (m_hInteropDevice)
		wglDXCloseDeviceNV(m_hInteropDevice);
}

bool CompositorWGL::InitInteropDevice()
{
	if (m_hInteropDevice)
		return true; // wglDXCloseDeviceNV(m_hInteropDevice);

	if (!gladInitialized)
	{
		if (!gladLoadGL())
			return false;
		if (!gladLoadWGL(wglGetCurrentDC()))
			return false;
#ifdef _DEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback((GLDEBUGPROC)DebugCallback, nullptr);
#else
		glDisable(GL_DEBUG_OUTPUT);
#endif // DEBUG
		glGetError(); // to clear the error caused deep in GLEW
		gladInitialized = GL_TRUE;
	}

	m_hInteropDevice = wglDXOpenDeviceNV(m_pDevice.Get());
	return m_hInteropDevice != nullptr;
}

TextureBase* CompositorWGL::CreateTexture()
{
	if (m_hInteropDevice)
		return new TextureWGL(m_pDevice.Get(), m_hInteropDevice);
	else
		return new TextureD3D(m_pDevice.Get());
}

void CompositorWGL::RenderTextureSwapChain(IDirect3DSurface surface, ovrTextureSwapChain swapChain, ovrRecti viewport, ovrEyeType eye)
{
	TextureWGL* texture = dynamic_cast<TextureWGL*>(swapChain->Textures[swapChain->SubmitIndex].get());
	if (texture)
		assert(texture->Unlock());
	CompositorD3D::RenderTextureSwapChain(surface, swapChain, viewport, eye);
	if (texture)
		assert(texture->Lock());
}
