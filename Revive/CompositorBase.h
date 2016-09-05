#pragma once

#include <OVR_CAPI.h>
#include <openvr.h>
#include <vector>

class CompositorBase
{
public:
	CompositorBase();
	virtual ~CompositorBase();

	virtual vr::EGraphicsAPIConvention GetAPI() = 0;

	// Texture Swapchain
	virtual ovrResult CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain) = 0;
	virtual void DestroyTextureSwapChain(ovrTextureSwapChain chain) = 0;

	// Mirror Texture
	virtual ovrResult CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture) = 0;
	virtual void DestroyMirrorTexture(ovrMirrorTexture mirrorTexture) = 0;
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture) = 0;

	vr::EVRCompositorError SubmitFrame(const ovrViewScaleDesc* viewScaleDesc, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);

protected:
	ovrMirrorTexture m_MirrorTexture;
	vr::VROverlayHandle_t CreateOverlay();

private:
	// Overlays
	unsigned int m_OverlayCount;
	std::vector<vr::VROverlayHandle_t> m_ActiveOverlays;
};
