#pragma once

#include "OVR_CAPI.h"

#include <openvr.h>

class TextureBase
{
public:
	TextureBase() { }
	virtual ~TextureBase() { };

	virtual vr::Texture_t ToVRTexture() = 0;
	virtual bool Create(int Width, int Height, int MipLevels, int ArraySize,
		ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags) = 0;
};
