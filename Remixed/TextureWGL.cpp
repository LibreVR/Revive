#include "TextureWGL.h"

TextureWGL::TextureWGL(ID3D11Device* pDevice, HANDLE interopDevice)
	: TextureD3D(pDevice)
	, m_hInteropDevice(interopDevice)
{
}

TextureWGL::~TextureWGL()
{
	wglDXUnregisterObjectNV(m_hInteropDevice, m_hInteropObject);
}

bool TextureWGL::Init(ovrTextureType type, int Width, int Height, int MipLevels, int ArraySize,
	int SampleCount, ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags)
{
	if (TextureD3D::Init(type, Width, Height, MipLevels, ArraySize, SampleCount, Format, MiscFlags, BindFlags))
	{
		glGenTextures(1, &m_InteropTexture);
		m_hInteropObject = wglDXRegisterObjectNV(m_hInteropDevice, m_pTexture.Get(), m_InteropTexture, GL_TEXTURE_2D, WGL_ACCESS_WRITE_DISCARD_NV);
		if (!m_hInteropObject)
			return false;

		return Lock();
	}
	return false;
}