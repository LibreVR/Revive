#pragma once

#include "Swapchain.h"

struct ovrTextureSwapChainVk : public ovrTextureSwapChainData
{
public:
	static ovrResult Create(ovrSession session, const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);

protected:
	static enum VkFormat TextureFormatToVkFormat(ovrTextureFormat format);
};
