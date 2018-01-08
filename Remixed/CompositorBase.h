#pragma once

#include "OVR_CAPI.h"
#include "TextureBase.h"

#include <vector>

class CompositorBase
{
public:
	CompositorBase();
	virtual ~CompositorBase();

	virtual void Flush() = 0;
	virtual TextureBase* CreateTexture() = 0;

	// Texture Swapchain
	ovrResult CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);
	virtual void RenderTextureSwapChain(ovrSession session, ovrEyeType eye, ovrTextureSwapChain swapChain, ovrRecti viewport) = 0;

	// Mirror Texture
	ovrResult CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture) = 0;

	ovrResult WaitToBeginFrame(ovrSession session, long long frameIndex);
	ovrResult BeginFrame(ovrSession session, long long frameIndex);
	ovrResult EndFrame(ovrSession session, long long frameIndex, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);

	void SetMirrorTexture(ovrMirrorTexture mirrorTexture);

protected:
	unsigned int m_ChainCount;
	ovrMirrorTexture m_MirrorTexture;

	ovrLayerEyeFov ToFovLayer(ovrLayerEyeMatrix* matrix);

	void SubmitFovLayer(ovrSession session, ovrLayerEyeFov* fovLayer);

private:
	// TODO: Overlays
	//unsigned int m_OverlayCount;
	//std::vector<vr::VROverlayHandle_t> m_ActiveOverlays;
};
