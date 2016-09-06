#pragma once
#include "CompositorBase.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <openvr.h>

class CompositorD3D :
	public CompositorBase
{
public:
	CompositorD3D(ID3D11Device* pDevice);
	virtual ~CompositorD3D();

	static CompositorD3D* Create(IUnknown* d3dPtr);
	virtual vr::EGraphicsAPIConvention GetAPI() { return vr::API_DirectX; };

	// Texture Swapchain
	virtual ovrResult CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);
	virtual void DestroyTextureSwapChain(ovrTextureSwapChain chain);

	// Mirror Texture
	virtual ovrResult CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture);
	virtual void DestroyMirrorTexture(ovrMirrorTexture mirrorTexture);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture);

protected:
	static DXGI_FORMAT TextureFormatToDXGIFormat(ovrTextureFormat format, unsigned int flags);
	static UINT BindFlagsToD3DBindFlags(unsigned int flags);
	static UINT MiscFlagsToD3DMiscFlags(unsigned int flags);

private:
	Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
};
