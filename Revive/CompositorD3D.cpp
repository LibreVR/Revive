#include "CompositorD3D.h"
#include "REV_Common.h"

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
	CompositorD3D* compositor = new CompositorD3D(pDevice);
	pDevice->Release();
	return compositor;
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
}

CompositorD3D::~CompositorD3D()
{
  if (m_MirrorTexture)
	DestroyMirrorTexture(m_MirrorTexture);
}

DXGI_FORMAT CompositorD3D::TextureFormatToDXGIFormat(ovrTextureFormat format, unsigned int flags)
{
	if (flags & ovrTextureMisc_DX_Typeless)
	{
		switch (format)
		{
			case OVR_FORMAT_UNKNOWN:              return DXGI_FORMAT_UNKNOWN;
			//case OVR_FORMAT_B5G6R5_UNORM:       return DXGI_FORMAT_B5G6R5_TYPELESS;
			//case OVR_FORMAT_B5G5R5A1_UNORM:     return DXGI_FORMAT_B5G5R5A1_TYPELESS;
			//case OVR_FORMAT_B4G4R4A4_UNORM:     return DXGI_FORMAT_B4G4R4A4_TYPELESS;
			case OVR_FORMAT_R8G8B8A8_UNORM:       return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:  return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case OVR_FORMAT_B8G8R8A8_UNORM:       return DXGI_FORMAT_B8G8R8A8_TYPELESS;
			case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:  return DXGI_FORMAT_B8G8R8A8_TYPELESS;
			case OVR_FORMAT_B8G8R8X8_UNORM:       return DXGI_FORMAT_B8G8R8X8_TYPELESS;
			case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return DXGI_FORMAT_B8G8R8X8_TYPELESS;
			case OVR_FORMAT_R16G16B16A16_FLOAT:   return DXGI_FORMAT_R16G16B16A16_TYPELESS;
			case OVR_FORMAT_D16_UNORM:            return DXGI_FORMAT_R16_TYPELESS;
			case OVR_FORMAT_D24_UNORM_S8_UINT:    return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			case OVR_FORMAT_D32_FLOAT:            return DXGI_FORMAT_R32_TYPELESS;
			case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

			// Added in 1.5 compressed formats can be used for static layers
			case OVR_FORMAT_BC1_UNORM:            return DXGI_FORMAT_BC1_TYPELESS;
			case OVR_FORMAT_BC1_UNORM_SRGB:       return DXGI_FORMAT_BC1_TYPELESS;
			case OVR_FORMAT_BC2_UNORM:            return DXGI_FORMAT_BC2_TYPELESS;
			case OVR_FORMAT_BC2_UNORM_SRGB:       return DXGI_FORMAT_BC2_TYPELESS;
			case OVR_FORMAT_BC3_UNORM:            return DXGI_FORMAT_BC3_TYPELESS;
			case OVR_FORMAT_BC3_UNORM_SRGB:       return DXGI_FORMAT_BC3_TYPELESS;
			case OVR_FORMAT_BC6H_UF16:            return DXGI_FORMAT_BC6H_TYPELESS;
			case OVR_FORMAT_BC6H_SF16:            return DXGI_FORMAT_BC6H_TYPELESS;
			case OVR_FORMAT_BC7_UNORM:            return DXGI_FORMAT_BC7_TYPELESS;
			case OVR_FORMAT_BC7_UNORM_SRGB:       return DXGI_FORMAT_BC7_TYPELESS;

			default: return DXGI_FORMAT_UNKNOWN;
		}
	}
	else
	{
		switch (format)
		{
			case OVR_FORMAT_UNKNOWN:              return DXGI_FORMAT_UNKNOWN;
			case OVR_FORMAT_B5G6R5_UNORM:         return DXGI_FORMAT_B5G6R5_UNORM;
			case OVR_FORMAT_B5G5R5A1_UNORM:       return DXGI_FORMAT_B5G5R5A1_UNORM;
			case OVR_FORMAT_B4G4R4A4_UNORM:       return DXGI_FORMAT_B4G4R4A4_UNORM;
			case OVR_FORMAT_R8G8B8A8_UNORM:       return DXGI_FORMAT_R8G8B8A8_UNORM;
			case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:  return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case OVR_FORMAT_B8G8R8A8_UNORM:       return DXGI_FORMAT_B8G8R8A8_UNORM;
			case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:  return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			case OVR_FORMAT_B8G8R8X8_UNORM:       return DXGI_FORMAT_B8G8R8X8_UNORM;
			case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
			case OVR_FORMAT_R16G16B16A16_FLOAT:   return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case OVR_FORMAT_D16_UNORM:            return DXGI_FORMAT_R16_TYPELESS;
			case OVR_FORMAT_D24_UNORM_S8_UINT:    return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			case OVR_FORMAT_D32_FLOAT:            return DXGI_FORMAT_R32_TYPELESS;
			case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

			// Added in 1.5 compressed formats can be used for static layers
			case OVR_FORMAT_BC1_UNORM:            return DXGI_FORMAT_BC1_UNORM;
			case OVR_FORMAT_BC1_UNORM_SRGB:       return DXGI_FORMAT_BC1_UNORM_SRGB;
			case OVR_FORMAT_BC2_UNORM:            return DXGI_FORMAT_BC2_UNORM;
			case OVR_FORMAT_BC2_UNORM_SRGB:       return DXGI_FORMAT_BC2_UNORM_SRGB;
			case OVR_FORMAT_BC3_UNORM:            return DXGI_FORMAT_BC3_UNORM;
			case OVR_FORMAT_BC3_UNORM_SRGB:       return DXGI_FORMAT_BC3_UNORM_SRGB;
			case OVR_FORMAT_BC6H_UF16:            return DXGI_FORMAT_BC6H_UF16;
			case OVR_FORMAT_BC6H_SF16:            return DXGI_FORMAT_BC6H_SF16;
			case OVR_FORMAT_BC7_UNORM:            return DXGI_FORMAT_BC7_UNORM;
			case OVR_FORMAT_BC7_UNORM_SRGB:       return DXGI_FORMAT_BC7_UNORM_SRGB;

			default: return DXGI_FORMAT_UNKNOWN;
		}
	}
}

UINT CompositorD3D::BindFlagsToD3DBindFlags(unsigned int flags)
{
	UINT result = D3D11_BIND_SHADER_RESOURCE;
	if (flags & ovrTextureBind_DX_RenderTarget)
		result |= D3D11_BIND_RENDER_TARGET;
	if (flags & ovrTextureBind_DX_UnorderedAccess)
		result |= D3D11_BIND_UNORDERED_ACCESS;
	if (flags & ovrTextureBind_DX_DepthStencil)
		result |= D3D11_BIND_DEPTH_STENCIL;
	return result;
}

UINT CompositorD3D::MiscFlagsToD3DMiscFlags(unsigned int flags)
{
	UINT result = 0;
	if (flags & ovrTextureMisc_AllowGenerateMips)
		result |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	return result;
}

HRESULT CompositorD3D::CreateTexture(UINT Width, UINT Height, UINT MipLevels, UINT ArraySize,
  ovrTextureFormat Format, UINT MiscFlags, UINT BindFlags, ID3D11Texture2D** Texture)
{
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = Width;
  desc.Height = Height;
  desc.MipLevels = MipLevels;
  desc.ArraySize = ArraySize;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Format = TextureFormatToDXGIFormat(Format, MiscFlags);
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = BindFlagsToD3DBindFlags(BindFlags);
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = MiscFlagsToD3DMiscFlags(MiscFlags);
  return m_pDevice->CreateTexture2D(&desc, nullptr, Texture);
}

ovrResult CompositorD3D::CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData(vr::API_DirectX, *desc);

	for (int i = 0; i < swapChain->Length; i++)
	{
		ID3D11Texture2D* texture;
		swapChain->Textures[i].eType = vr::API_DirectX;
		swapChain->Textures[i].eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
		HRESULT hr = CreateTexture(desc->Width, desc->Height, desc->MipLevels, desc->ArraySize, desc->Format,
		  desc->MiscFlags, desc->BindFlags, &texture);
		if (FAILED(hr))
			return ovrError_RuntimeException;

		D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
		view_desc.Format = TextureFormatToDXGIFormat(desc->Format, 0);
		view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		view_desc.Texture2D.MipLevels = desc->MipLevels;
		view_desc.Texture2D.MostDetailedMip = 0;
		hr = m_pDevice->CreateShaderResourceView(texture, &view_desc, (ID3D11ShaderResourceView**)&swapChain->Views[i]);
		if (FAILED(hr))
			return ovrError_RuntimeException;

		if (desc->BindFlags & ovrTextureBind_DX_RenderTarget)
		{
			D3D11_RENDER_TARGET_VIEW_DESC target_desc;
			target_desc.Format = TextureFormatToDXGIFormat(desc->Format, 0);
			target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			target_desc.Texture2D.MipSlice = 0;
			hr = m_pDevice->CreateRenderTargetView(texture, &target_desc, (ID3D11RenderTargetView**)&swapChain->Targets[i]);
			if (FAILED(hr))
				return ovrError_RuntimeException;
		}

		swapChain->Textures[i].handle = texture;
	}

	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

void CompositorD3D::DestroyTextureSwapChain(ovrTextureSwapChain chain)
{
	for (int i = 0; i < chain->Length; i++)
	{
		((ID3D11Texture2D*)chain->Textures[i].handle)->Release();
		((ID3D11ShaderResourceView*)chain->Textures[i].handle)->Release();
		((ID3D11Texture2D*)chain->Textures[i].handle)->Release();
	}
}

ovrResult CompositorD3D::CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture)
{
	// There can only be one mirror texture at a time
	if (m_MirrorTexture)
		return ovrError_RuntimeException;

	// TODO: Implement support for texture flags.
	ID3D11Texture2D* texture;
	HRESULT hr = CreateTexture(desc->Width, desc->Height, 1, 1, desc->Format,
	  desc->MiscFlags | ovrTextureMisc_AllowGenerateMips, ovrTextureBind_DX_RenderTarget, &texture);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData(vr::API_DirectX, *desc);
	mirrorTexture->Texture.handle = texture;
	mirrorTexture->Texture.eType = vr::API_DirectX;
	mirrorTexture->Texture.eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.

	// Create a render target for the mirror texture.
	D3D11_RENDER_TARGET_VIEW_DESC rdesc;
	rdesc.Format = TextureFormatToDXGIFormat(desc->Format, 0);
	rdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rdesc.Texture2D.MipSlice = 0;
	hr = m_pDevice->CreateRenderTargetView(texture, &rdesc, (ID3D11RenderTargetView**)&mirrorTexture->Target);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	// Get the mirror textures
	vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Left, m_pDevice.Get(), &mirrorTexture->Views[ovrEye_Left]);
	vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Right, m_pDevice.Get(), &mirrorTexture->Views[ovrEye_Right]);

	m_MirrorTexture = mirrorTexture;
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

void CompositorD3D::DestroyMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	((ID3D11Texture2D*)mirrorTexture->Texture.handle)->Release();
	((ID3D11RenderTargetView*)mirrorTexture->Target)->Release();
	((ID3D11ShaderResourceView*)mirrorTexture->Views[ovrEye_Left])->Release();
	((ID3D11ShaderResourceView*)mirrorTexture->Views[ovrEye_Right])->Release();
	m_MirrorTexture = nullptr;
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

	// Set the mirror shaders
	m_pContext->VSSetShader(m_VertexShader.Get(), NULL, 0);
	m_pContext->PSSetShader(m_MirrorShader.Get(), NULL, 0);
	m_pContext->PSSetShaderResources(0, 2, (ID3D11ShaderResourceView**)mirrorTexture->Views);

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
	m_pContext->ClearRenderTargetView((ID3D11RenderTargetView*)mirrorTexture->Target, clear);
	m_pContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&mirrorTexture->Target, NULL);
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

	// Set the texture resource and generate mips if allowed
	if (swapChain->Desc.MiscFlags & ovrTextureMisc_AllowGenerateMips)
		m_pContext->GenerateMips((ID3D11ShaderResourceView*)swapChain->SubmittedView);
	m_pContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&swapChain->SubmittedView);

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
	D3D11_VIEWPORT vp = { viewport.Pos.x, viewport.Pos.y, viewport.Size.w, viewport.Size.h, D3D11_MIN_DEPTH, D3D11_MIN_DEPTH };
	m_pContext->RSSetViewports(1, &vp);
	m_pContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&sceneChain->SubmittedTarget, nullptr);
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
