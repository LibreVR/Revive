#pragma once

#include "Swapchain.h"

struct ovrTextureSwapChainD3D11 : public ovrTextureSwapChainData
{
public:
	static ovrResult Create(ovrSession session, const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);
};
