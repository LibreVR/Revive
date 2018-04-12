#pragma once

#include "OVR_CAPI.h"
#include "TextureBase.h"

#include <vector>

#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

class CompositorBase
{
public:
	CompositorBase();
	virtual ~CompositorBase();

	virtual void Flush() = 0;
	virtual TextureBase* CreateTexture() = 0;

	// Texture Swapchain
	ovrResult CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);
	virtual void RenderTextureSwapChain(winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface surface,
		ovrTextureSwapChain swapChain, ovrRecti viewport, ovrEyeType eye = ovrEye_Left) = 0;

	// Mirror Texture
	ovrResult CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture) = 0;

	ovrResult WaitToBeginFrame(ovrSession session, long long frameIndex);
	ovrResult EndFrame(ovrSession session, long long frameIndex, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);

	void SetMirrorTexture(ovrMirrorTexture mirrorTexture);

protected:
	unsigned int m_ChainCount;
	ovrMirrorTexture m_MirrorTexture;

	ovrLayerEyeFov ToFovLayer(ovrLayerEyeMatrix* matrix);

	void SubmitFovLayer(winrt::Windows::Graphics::Holographic::HolographicFrame frame, ovrLayerEyeFov* fovLayer);

private:
	// TODO: Overlays
	//unsigned int m_OverlayCount;
	//std::vector<vr::VROverlayHandle_t> m_ActiveOverlays;
};
