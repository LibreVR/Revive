#pragma once

#include "Swapchain.h"

struct ovrTextureSwapChainGL : public ovrTextureSwapChainData
{
public:
	static ovrResult Create(ovrSession session, const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);

protected:
	static unsigned int TextureFormatToGLFormat(ovrTextureFormat format);
};
