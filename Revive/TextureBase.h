#pragma once

#include "OVR_CAPI.h"
#include "openvr.h"

#include <memory>

#define REV_SWAPCHAIN_LENGTH 2

class TextureBase
{
public:
	TextureBase() { }
	virtual ~TextureBase() { };

	virtual vr::Texture_t ToVRTexture() = 0;
	virtual bool Create(int Width, int Height, int MipLevels, int ArraySize,
		ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags) = 0;
};

struct ovrTextureSwapChainData
{
	ovrTextureSwapChainDesc Desc;
	vr::ETextureType ApiType;
	vr::VROverlayHandle_t Overlay;

	unsigned int Identifier;
	int Length, CurrentIndex, SubmitIndex;
	std::unique_ptr<TextureBase> Textures[REV_SWAPCHAIN_LENGTH];

	bool Full() { return (CurrentIndex + 1) % Length == SubmitIndex; }
	void Commit() { CurrentIndex++; CurrentIndex %= Length; };
	void Submit() { SubmitIndex++; SubmitIndex %= Length; };

	ovrTextureSwapChainData(vr::ETextureType api, ovrTextureSwapChainDesc desc);
};

struct ovrMirrorTextureData
{
	ovrMirrorTextureDesc Desc;
	vr::ETextureType ApiType;
	std::unique_ptr<TextureBase> Texture;

	ovrMirrorTextureData(vr::ETextureType api, ovrMirrorTextureDesc desc);
};
