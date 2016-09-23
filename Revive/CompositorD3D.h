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
	virtual void RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad);

	// Mirror Texture
	virtual ovrResult CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture, ovrTextureSwapChain swapChain[ovrEye_Count]);

protected:
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

	// Mirror
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pMirror[ovrEye_Count];
};
