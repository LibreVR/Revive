#include "TextureBase.h"

ovrTextureSwapChainData::ovrTextureSwapChainData(ovrTextureSwapChainDesc desc)
	: Length(REV_SWAPCHAIN_MAX_LENGTH)
	, Identifier(0)
	, CurrentIndex(0)
	, SubmitIndex(0)
	, Desc(desc)
	, Overlay(vr::k_ulOverlayHandleInvalid)
	, Textures()
{
}

ovrTextureSwapChainData::~ovrTextureSwapChainData()
{
}

ovrMirrorTextureData::ovrMirrorTextureData(ovrMirrorTextureDesc desc)
	: Desc(desc)
{
}

ovrMirrorTextureData::~ovrMirrorTextureData()
{
}
