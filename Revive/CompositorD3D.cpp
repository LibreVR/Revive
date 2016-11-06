#include "CompositorD3D.h"
#include "Common.h"
#include "TextureD3D.h"

#include <openvr.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "VertexShader.hlsl.h"
#include "MirrorShader.hlsl.h"
#include "CompositorShader.hlsl.h"

struct Vertex
{
	ovrVector2f Position;
	ovrVector2f TexCoord;
};

CompositorD3D* CompositorD3D::Create(IUnknown* d3dPtr)
{
	// Get the device for this context
	// TODO: DX12 support
	ID3D11Device* pDevice;
	HRESULT hr = d3dPtr->QueryInterface(&pDevice);
	if (FAILED(hr))
		return nullptr;

	// Attach compositor to the device
	return new CompositorD3D(pDevice);
}

CompositorD3D::CompositorD3D(ID3D11Device* pDevice)
{
	m_pDevice = pDevice;
	m_pDevice->GetImmediateContext(m_pContext.GetAddressOf());

	// Create the shaders.
	m_pDevice->CreateVertexShader(g_VertexShader, sizeof(g_VertexShader), NULL, m_VertexShader.GetAddressOf());
	m_pDevice->CreatePixelShader(g_MirrorShader, sizeof(g_MirrorShader), NULL, m_MirrorShader.GetAddressOf());
	m_pDevice->CreatePixelShader(g_CompositorShader, sizeof(g_CompositorShader), NULL, m_CompositorShader.GetAddressOf());

	// Create the vertex buffer.
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(Vertex) * 4;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	m_pDevice->CreateBuffer(&bufferDesc, nullptr, m_VertexBuffer.GetAddressOf());

	// Create the input layout.
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
		D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	HRESULT hr = m_pDevice->CreateInputLayout(layout, 2, g_VertexShader, sizeof(g_VertexShader), m_InputLayout.GetAddressOf());

	// Create state objects.
	D3D11_BLEND_DESC bm = { 0 };
	bm.RenderTarget[0].BlendEnable = true;
	bm.RenderTarget[0].BlendOp = bm.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bm.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	bm.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bm.RenderTarget[0].SrcBlendAlpha = bm.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bm.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	m_pDevice->CreateBlendState(&bm, m_BlendState.GetAddressOf());

	// Get the mirror textures
	ID3D11ShaderResourceView *left, *right;
	vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Left, m_pDevice.Get(), (void**)&left);
	vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Right, m_pDevice.Get(), (void**)&right);

	// OpenVR doesn't increment the reference counter as it should, so we have to do it ourselves
	left->AddRef();
	right->AddRef();
	m_pMirror[ovrEye_Left] = left;
	m_pMirror[ovrEye_Right] = right;
}

CompositorD3D::~CompositorD3D()
{
}

ovrResult CompositorD3D::CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData(vr::API_DirectX, *desc);

	for (int i = 0; i < swapChain->Length; i++)
	{
		TextureD3D* texture = new TextureD3D(m_pDevice.Get());
		bool success = texture->Create(desc->Width, desc->Height, desc->MipLevels, desc->ArraySize, desc->Format,
			desc->MiscFlags, desc->BindFlags);
		if (!success)
			return ovrError_RuntimeException;
		swapChain->Textures[i].reset(texture);
	}

	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

ovrResult CompositorD3D::CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture)
{
	// There can only be one mirror texture at a time
	if (m_MirrorTexture)
		return ovrError_RuntimeException;

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData(vr::API_DirectX, *desc);
	TextureD3D* texture = new TextureD3D(m_pDevice.Get());
	bool success = texture->Create(desc->Width, desc->Height, 1, 1, desc->Format,
		desc->MiscFlags | ovrTextureMisc_AllowGenerateMips, ovrTextureBind_DX_RenderTarget);
	if (!success)
		return ovrError_RuntimeException;
	mirrorTexture->Texture.reset(texture);

	m_MirrorTexture = mirrorTexture;
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

void CompositorD3D::RenderMirrorTexture(ovrMirrorTexture mirrorTexture, ovrTextureSwapChain swapChain[ovrEye_Count])
{
	// Get the current state objects
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state;
	float blend_factor[4];
	uint32_t sample_mask;
	m_pContext->OMGetBlendState(blend_state.GetAddressOf(), blend_factor, &sample_mask);

	D3D11_PRIMITIVE_TOPOLOGY topology;
	m_pContext->IAGetPrimitiveTopology(&topology);

	ID3D11RasterizerState* ras_state;
	m_pContext->RSGetState(&ras_state);

	// Get the mirror texture
	TextureD3D* texture = (TextureD3D*)mirrorTexture->Texture.get();

	// Set the mirror shaders
	m_pContext->VSSetShader(m_VertexShader.Get(), NULL, 0);
	m_pContext->PSSetShader(m_MirrorShader.Get(), NULL, 0);
	ID3D11ShaderResourceView* resources[] = { m_pMirror[ovrEye_Left].Get(), m_pMirror[ovrEye_Right].Get() };
	m_pContext->PSSetShaderResources(0, 2, resources);

	// Update the vertex buffer
	Vertex vertices[4] = {
		{ { -1.0f,  1.0f },{ 0.0f, 0.0f } },
		{ {  1.0f,  1.0f },{ 1.0f, 0.0f } },
		{ { -1.0f, -1.0f },{ 0.0f, 1.0f } },
		{ {  1.0f, -1.0f },{ 1.0f, 1.0f } }
	};
	D3D11_MAPPED_SUBRESOURCE map = { 0 };
	m_pContext->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	memcpy(map.pData, vertices, sizeof(Vertex) * 4);
	m_pContext->Unmap(m_VertexBuffer.Get(), 0);

	// Prepare the render target
	FLOAT clear[4] = { 0.0f };
	D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)mirrorTexture->Desc.Width, (float)mirrorTexture->Desc.Height, D3D11_MIN_DEPTH, D3D11_MIN_DEPTH };
	m_pContext->RSSetViewports(1, &viewport);

	ID3D11RenderTargetView* target = texture->Target();
	m_pContext->ClearRenderTargetView(target, clear);
	m_pContext->OMSetRenderTargets(1, &target, NULL);
	m_pContext->OMSetBlendState(nullptr, nullptr, -1);
	m_pContext->RSSetState(nullptr);

	// Set and draw the vertices
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pContext->IASetInputLayout(m_InputLayout.Get());
	m_pContext->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	m_pContext->Draw(4, 0);

	// Restore the state objects
	m_pContext->RSSetState(ras_state);
	m_pContext->OMSetBlendState(blend_state.Get(), blend_factor, sample_mask);
	m_pContext->IASetPrimitiveTopology(topology);
}

void CompositorD3D::RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad)
{
	uint32_t width, height;
	vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);

	// Get the current state objects
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state;
	float blend_factor[4];
	uint32_t sample_mask;
	m_pContext->OMGetBlendState(blend_state.GetAddressOf(), blend_factor, &sample_mask);

	D3D11_PRIMITIVE_TOPOLOGY topology;
	m_pContext->IAGetPrimitiveTopology(&topology);

	ID3D11RasterizerState* ras_state;
	m_pContext->RSGetState(&ras_state);

	// Set the compositor shaders
	m_pContext->VSSetShader(m_VertexShader.Get(), NULL, 0);
	m_pContext->PSSetShader(m_CompositorShader.Get(), NULL, 0);
	ID3D11ShaderResourceView* resource = ((TextureD3D*)swapChain->Submitted)->Resource();
	m_pContext->PSSetShaderResources(0, 1, &resource);

	// Update the vertex buffer
	Vertex vertices[4] = {
		{ { quad.v[0], quad.v[2] },{ bounds.uMin, bounds.vMin } },
		{ { quad.v[1], quad.v[2] },{ bounds.uMax, bounds.vMin } },
		{ { quad.v[0], quad.v[3] },{ bounds.uMin, bounds.vMax } },
		{ { quad.v[1], quad.v[3] },{ bounds.uMax, bounds.vMax } }
	};
	D3D11_MAPPED_SUBRESOURCE map = { 0 };
	m_pContext->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	memcpy(map.pData, vertices, sizeof(Vertex) * 4);
	m_pContext->Unmap(m_VertexBuffer.Get(), 0);

	// Prepare the render target
	D3D11_VIEWPORT vp = { (float)viewport.Pos.x, (float)viewport.Pos.y, (float)viewport.Size.w, (float)viewport.Size.h, D3D11_MIN_DEPTH, D3D11_MIN_DEPTH };
	m_pContext->RSSetViewports(1, &vp);
	ID3D11RenderTargetView* target = ((TextureD3D*)sceneChain->Submitted)->Target();
	m_pContext->OMSetRenderTargets(1, &target, nullptr);
	m_pContext->OMSetBlendState(m_BlendState.Get(), nullptr, -1);
	m_pContext->RSSetState(nullptr);

	// Set and draw the vertices
	uint32_t stride = sizeof(Vertex);
	uint32_t offset = 0;
	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pContext->IASetInputLayout(m_InputLayout.Get());
	m_pContext->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	m_pContext->Draw(4, 0);

	// Restore the state objects
	m_pContext->RSSetState(ras_state);
	m_pContext->OMSetBlendState(blend_state.Get(), blend_factor, sample_mask);
	m_pContext->IASetPrimitiveTopology(topology);
}
