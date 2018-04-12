#pragma once

#include "CompositorBase.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

class CompositorD3D :
	public CompositorBase
{
public:
	CompositorD3D();
	CompositorD3D(IUnknown* pDevice);
	virtual ~CompositorD3D();

	bool InitDevice();
	winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice GetDevice();
	bool SetDevice(IUnknown* pDevice);

	virtual void Flush() { if (m_pContext) m_pContext->Flush(); };
	virtual TextureBase* CreateTexture();

	virtual void RenderTextureSwapChain(winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface surface,
		ovrTextureSwapChain swapChain, ovrRecti viewport, ovrEyeType eye);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture);

protected:
	// DirectX 11
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

	// TODO: Mirror
};
