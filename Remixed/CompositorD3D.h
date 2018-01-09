#pragma once

#include "CompositorBase.h"

#include <d3d11.h>
#include <wrl/client.h>

class CompositorD3D :
	public CompositorBase
{
public:
	CompositorD3D(ID3D11Device* pDevice);
	virtual ~CompositorD3D();

	static CompositorD3D* Create(IUnknown* d3dPtr);
	virtual void Flush() { if (m_pContext) m_pContext->Flush(); };
	virtual TextureBase* CreateTexture();

	virtual void RenderTextureSwapChain(ovrSession session, long long frameIndex, ovrEyeType eye, ovrTextureSwapChain swapChain, ovrRecti viewport);
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
