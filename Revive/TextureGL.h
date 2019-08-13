#pragma once

#include "TextureBase.h"

#include <glad/glad.h>

class TextureGL :
	public TextureBase
{
public:
	TextureGL();
	virtual ~TextureGL();

	virtual void ToVRTexture(vr::Texture_t& texture);
	virtual bool Init(ovrTextureType type, int Width, int Height, int MipLevels, int ArraySize,
		ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags);

	GLuint Texture;
	GLuint Framebuffer;

protected:
	static GLenum TextureFormatToInternalFormat(ovrTextureFormat format);
	static GLenum TextureFormatToGLFormat(ovrTextureFormat format);
};
