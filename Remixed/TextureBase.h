#pragma once

#include "OVR_CAPI.h"

#include <memory>

#include <winrt/Windows.Graphics.Holographic.h>

#define REV_SWAPCHAIN_MAX_LENGTH 3

class TextureBase
{
public:
	TextureBase() { }
	virtual ~TextureBase() { };

	virtual bool Init(ovrTextureType type, int width, int height, int mipLevels, int arraySize,
		int sampleCount, ovrTextureFormat format, unsigned int miscFlags, unsigned int bindFlags) = 0;

	static bool IsDepthFormat(ovrTextureFormat format);
	static bool IsSRGBFormat(ovrTextureFormat format);
};

struct ovrTextureSwapChainData
{
	ovrTextureSwapChainDesc Desc;

	unsigned int Identifier;
	int Length, CurrentIndex, SubmitIndex;
	std::unique_ptr<TextureBase> Textures[REV_SWAPCHAIN_MAX_LENGTH];

	bool Full() { return (CurrentIndex + 1) % Length == SubmitIndex; }
	void Commit() { CurrentIndex++; CurrentIndex %= Length; };
	void Submit() { SubmitIndex = CurrentIndex; };

	ovrTextureSwapChainData(ovrTextureSwapChainDesc desc);
	~ovrTextureSwapChainData();
};

struct ovrMirrorTextureData
{
	ovrMirrorTextureDesc Desc;
	std::unique_ptr<TextureBase> Texture;

	ovrMirrorTextureData(ovrMirrorTextureDesc desc);
	~ovrMirrorTextureData();
};
