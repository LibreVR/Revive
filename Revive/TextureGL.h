#pragma once

#include "TextureBase.h"

#include <GL/glew.h>

class TextureGL :
	public TextureBase
{
public:
	TextureGL();
	virtual ~TextureGL();

	virtual vr::Texture_t ToVRTexture();
	virtual bool Create(int Width, int Height, int MipLevels, int ArraySize,
		ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags);

	GLuint Texture;
	GLuint Framebuffer;

protected:
	static GLenum TextureFormatToInternalFormat(ovrTextureFormat format);
	static GLenum TextureFormatToGLFormat(ovrTextureFormat format);
};
