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
	virtual void RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad);

	// Mirror Texture
	virtual ovrResult CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture);
	virtual void DestroyMirrorTexture(ovrMirrorTexture mirrorTexture);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture, ovrTextureSwapChain swapChain[ovrEye_Count]);

protected:
	static DXGI_FORMAT TextureFormatToDXGIFormat(ovrTextureFormat format, unsigned int flags);
	static UINT BindFlagsToD3DBindFlags(unsigned int flags);
	static UINT MiscFlagsToD3DMiscFlags(unsigned int flags);
	HRESULT CreateTexture(UINT Width, UINT Height, UINT MipLevels, UINT ArraySize,
	  ovrTextureFormat Format, UINT MiscFlags, UINT BindFlags, ID3D11Texture2D** Texture);

	Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pContext;

	// Shaders
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_MirrorShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_CompositorShader;

	// Input
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_VertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout;

	// States
	Microsoft::WRL::ComPtr<ID3D11BlendState> m_BlendState;
};
