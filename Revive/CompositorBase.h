#pragma once

#include "TextureBase.h"
#include "OVR_CAPI.h"

#include <openvr.h>
#include <vector>

class CompositorBase
{
public:
	CompositorBase();
	virtual ~CompositorBase();

	virtual vr::ETextureType GetAPI() = 0;
	virtual void Flush() = 0;
	virtual TextureBase* CreateTexture() = 0;

	// Texture Swapchain
	ovrResult CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);
	virtual void RenderTextureSwapChain(vr::EVREye eye, TextureBase* src, TextureBase* dst, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad) = 0;

	// Mirror Texture
	ovrResult CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture) = 0;

	ovrResult WaitToBeginFrame(ovrSession session, long long frameIndex);
	ovrResult BeginFrame(ovrSession session, long long frameIndex);
	ovrResult EndFrame(ovrSession session, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);

	void SetMirrorTexture(ovrMirrorTexture mirrorTexture);
	static vr::VRTextureBounds_t FovPortToTextureBounds(ovrFovPort eyeFov, ovrFovPort fov);

protected:
	unsigned int m_ChainCount;
	ovrMirrorTexture m_MirrorTexture;

	vr::VROverlayHandle_t CreateOverlay();
	vr::VRTextureBounds_t ViewportToTextureBounds(ovrRecti viewport, ovrTextureSwapChain swapChain, unsigned int flags);

	const ovrLayer_Union& ToUnion(const ovrLayerHeader* layerPtr);

	void BlitLayers(const ovrLayerHeader* dstLayer, const ovrLayerHeader* srcLayer);
	vr::VRCompositorError SubmitLayer(ovrSession session, const ovrLayerHeader* baseLayer);

private:
	// Overlays
	unsigned int m_OverlayCount;
	std::vector<vr::VROverlayHandle_t> m_ActiveOverlays;

	// Call order enforcement
	void* m_FrameEvent;
};
