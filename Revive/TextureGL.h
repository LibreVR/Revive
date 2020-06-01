#pragma once

#include "TextureBase.h"

class TextureGL :
	public TextureBase
{
public:
	TextureGL();
	virtual ~TextureGL();

	virtual void ToVRTexture(vr::Texture_t& texture);
	virtual bool Init(ovrTextureType type, int Width, int Height, int MipLevels, int ArraySize,
		ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags);

	virtual bool CreateSharedTextureGL(unsigned int* outName) override;
	virtual void DeleteSharedTextureGL(unsigned int name) override;

	unsigned int Texture;
	unsigned int Framebuffer;

protected:
	static unsigned int TextureFormatToInternalFormat(ovrTextureFormat format);
	static unsigned int TextureFormatToGLFormat(ovrTextureFormat format);

	int m_Width, m_Height;
	unsigned int m_InteropTexture;
};
