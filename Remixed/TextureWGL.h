#pragma once

#include "TextureD3D.h"

#include <glad/glad.h>
#include <glad/glad_wgl.h>

class TextureWGL :
	public TextureD3D
{
public:
	TextureWGL(ID3D11Device* pDevice, HANDLE interopDevice);
	virtual ~TextureWGL();

	virtual bool Init(ovrTextureType type, int Width, int Height, int MipLevels, int ArraySize,
		int sampleCount, ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags);

	GLuint InteropTexture() { return m_InteropTexture; };
	bool Lock() { return wglDXLockObjectsNV(m_hInteropDevice, 1, &m_hInteropObject); }
	bool Unlock() { return wglDXUnlockObjectsNV(m_hInteropDevice, 1, &m_hInteropObject); }

protected:
	GLuint m_InteropTexture;

	HANDLE m_hInteropDevice;
	HANDLE m_hInteropObject;
};

