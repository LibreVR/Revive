#include "OVR_CAPI_D3D.h"

#include "openvr.h"
#include <d3d11.h>

#include "REV_Assert.h"
#include "REV_Common.h"

#include "MirrorVS.hlsl.h"
#include "MirrorPS.hlsl.h"

ID3D11VertexShader* g_pMirrorVS = nullptr;
ID3D11PixelShader* g_pMirrorPS = nullptr;

DXGI_FORMAT ovr_TextureFormatToDXGIFormat(ovrTextureFormat format, unsigned int flags)
{
	if (flags & ovrTextureMisc_DX_Typeless)
	{
		switch (format)
		{
			case OVR_FORMAT_UNKNOWN:				return DXGI_FORMAT_UNKNOWN;
			//case OVR_FORMAT_B5G6R5_UNORM:			return DXGI_FORMAT_B5G6R5_TYPELESS;
			//case OVR_FORMAT_B5G5R5A1_UNORM:		return DXGI_FORMAT_B5G5R5A1_TYPELESS;
			//case OVR_FORMAT_B4G4R4A4_UNORM:		return DXGI_FORMAT_B4G4R4A4_TYPELESS;
			case OVR_FORMAT_R8G8B8A8_UNORM:			return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:	return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case OVR_FORMAT_B8G8R8A8_UNORM:			return DXGI_FORMAT_B8G8R8A8_TYPELESS;
			case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:	return DXGI_FORMAT_B8G8R8A8_TYPELESS;
			case OVR_FORMAT_B8G8R8X8_UNORM:			return DXGI_FORMAT_B8G8R8X8_TYPELESS;
			case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:	return DXGI_FORMAT_B8G8R8X8_TYPELESS;
			case OVR_FORMAT_R16G16B16A16_FLOAT:		return DXGI_FORMAT_R16G16B16A16_TYPELESS;
			case OVR_FORMAT_D16_UNORM:				return DXGI_FORMAT_R16_TYPELESS;
			case OVR_FORMAT_D24_UNORM_S8_UINT:		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			case OVR_FORMAT_D32_FLOAT:				return DXGI_FORMAT_R32_TYPELESS;
			case OVR_FORMAT_D32_FLOAT_S8X24_UINT:	return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
			default: return DXGI_FORMAT_UNKNOWN;
		}
	}
	else
	{
		switch (format)
		{
			case OVR_FORMAT_UNKNOWN:				return DXGI_FORMAT_UNKNOWN;
			case OVR_FORMAT_B5G6R5_UNORM:			return DXGI_FORMAT_B5G6R5_UNORM;
			case OVR_FORMAT_B5G5R5A1_UNORM:			return DXGI_FORMAT_B5G5R5A1_UNORM;
			case OVR_FORMAT_B4G4R4A4_UNORM:			return DXGI_FORMAT_B4G4R4A4_UNORM;
			case OVR_FORMAT_R8G8B8A8_UNORM:			return DXGI_FORMAT_R8G8B8A8_UNORM;
			case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:	return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case OVR_FORMAT_B8G8R8A8_UNORM:			return DXGI_FORMAT_B8G8R8A8_UNORM;
			case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:	return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			case OVR_FORMAT_B8G8R8X8_UNORM:			return DXGI_FORMAT_B8G8R8X8_UNORM;
			case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:	return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
			case OVR_FORMAT_R16G16B16A16_FLOAT:		return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case OVR_FORMAT_D16_UNORM:				return DXGI_FORMAT_R16_TYPELESS;
			case OVR_FORMAT_D24_UNORM_S8_UINT:		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			case OVR_FORMAT_D32_FLOAT:				return DXGI_FORMAT_R32_TYPELESS;
			case OVR_FORMAT_D32_FLOAT_S8X24_UINT:	return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
			default: return DXGI_FORMAT_UNKNOWN;
		}
	}
}

UINT ovr_BindFlagsToD3DBindFlags(unsigned int flags)
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

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainDX(ovrSession session,
                                                            IUnknown* d3dPtr,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	if (!session)
		return ovrError_InvalidSession;

	if (!d3dPtr || !desc || !out_TextureSwapChain || desc->Type != ovrTexture_2D)
		return ovrError_InvalidParameter;

	// TODO: DX12 support.
	ID3D11Device* pDevice;
	HRESULT hr = d3dPtr->QueryInterface(&pDevice);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	D3D11_TEXTURE2D_DESC tdesc;
	tdesc.Width = desc->Width;
	tdesc.Height = desc->Height;
	tdesc.MipLevels = desc->MipLevels;
	tdesc.ArraySize = desc->ArraySize;
	tdesc.SampleDesc.Count = 1;
	tdesc.SampleDesc.Quality = 0;
	tdesc.Format = ovr_TextureFormatToDXGIFormat(desc->Format, desc->MiscFlags);
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.BindFlags = ovr_BindFlagsToD3DBindFlags(desc->BindFlags);
	tdesc.CPUAccessFlags = 0;
	tdesc.MiscFlags = 0;

	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData();
	swapChain->length = 2;
	swapChain->index = 0;
	swapChain->desc = *desc;

	for (int i = 0; i < swapChain->length; i++)
	{
		ID3D11Texture2D* texture;
		swapChain->texture[i].eType = vr::API_DirectX;
		swapChain->texture[i].eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
		hr = pDevice->CreateTexture2D(&tdesc, nullptr, &texture);
		if (FAILED(hr))
			return ovrError_RuntimeException;

		swapChain->texture[i].handle = texture;
	}
	swapChain->current = swapChain->texture[swapChain->index];

	// Clean up and return
	pDevice->Release();
	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainBufferDX(ovrSession session,
                                                               ovrTextureSwapChain chain,
                                                               int index,
                                                               IID iid,
                                                               void** out_Buffer)
{
	if (!session)
		return ovrError_InvalidSession;

	if (!chain || !out_Buffer)
		return ovrError_InvalidParameter;

	ID3D11Texture2D* texturePtr = (ID3D11Texture2D*)chain->texture[index].handle;
	HRESULT hr = texturePtr->QueryInterface(iid, out_Buffer);
	if (FAILED(hr))
		return ovrError_InvalidParameter;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureDX(ovrSession session,
                                                         IUnknown* d3dPtr,
                                                         const ovrMirrorTextureDesc* desc,
                                                         ovrMirrorTexture* out_MirrorTexture)
{
	if (!session)
		return ovrError_InvalidSession;

	if (!d3dPtr || !desc || !out_MirrorTexture)
		return ovrError_InvalidParameter;

	// TODO: DX12 support.
	ID3D11Device* pDevice;
	HRESULT hr = d3dPtr->QueryInterface(&pDevice);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	// TODO: Implement support for texture flags.
	ID3D11Texture2D* texture;
	D3D11_TEXTURE2D_DESC tdesc;
	tdesc.Width = desc->Width;
	tdesc.Height = desc->Height;
	tdesc.MipLevels = 1;
	tdesc.ArraySize = 1;
	tdesc.SampleDesc.Count = 1;
	tdesc.SampleDesc.Quality = 0;
	tdesc.Format = ovr_TextureFormatToDXGIFormat(desc->Format, desc->MiscFlags);
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	tdesc.CPUAccessFlags = 0;
	tdesc.MiscFlags = 0;
	hr = pDevice->CreateTexture2D(&tdesc, nullptr, &texture);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData();
	mirrorTexture->desc = *desc;
	mirrorTexture->texture.handle = texture;
	mirrorTexture->texture.eType = vr::API_DirectX;
	mirrorTexture->texture.eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.

	// Create a render target for the mirror texture.
	pDevice->CreateRenderTargetView(texture, NULL, (ID3D11RenderTargetView**)&mirrorTexture->target);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	// Clean up and return
	pDevice->Release();
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetMirrorTextureBufferDX(ovrSession session,
                                                            ovrMirrorTexture mirrorTexture,
                                                            IID iid,
                                                            void** out_Buffer)
{
	if (!session)
		return ovrError_InvalidSession;

	if (!mirrorTexture || !out_Buffer)
		return ovrError_InvalidParameter;

	ID3D11Texture2D* texture = (ID3D11Texture2D*)mirrorTexture->texture.handle;

	ID3D11Device* pDevice;
	texture->GetDevice(&pDevice);
	ID3D11DeviceContext* pContext;
	pDevice->GetImmediateContext(&pContext);

	// Compile and set vertex shader
	if (!g_pMirrorVS)
	{
		HRESULT hr = pDevice->CreateVertexShader(g_MirrorVS, sizeof(g_MirrorVS), NULL, &g_pMirrorVS);
		if (FAILED(hr))
			return ovrError_RuntimeException;
	}
	pContext->VSSetShader(g_pMirrorVS, NULL, 0);

	// Compile and set pixel shader
	if (!g_pMirrorPS)
	{
		HRESULT hr = pDevice->CreatePixelShader(g_MirrorPS, sizeof(g_MirrorPS), NULL, &g_pMirrorPS);
		if (FAILED(hr))
			return ovrError_RuntimeException;
	}
	pContext->PSSetShader(g_pMirrorPS, NULL, 0);

	ID3D11ShaderResourceView* mirrorViews[2];
	session->compositor->GetMirrorTextureD3D11(vr::Eye_Left, pDevice, (void**)&mirrorViews[0]);
	session->compositor->GetMirrorTextureD3D11(vr::Eye_Right, pDevice, (void**)&mirrorViews[1]);
	pContext->PSSetShaderResources(0, 2, mirrorViews);

	// Draw a triangle strip, the vertex buffer is not used.
	FLOAT clear[4] = { 0.0f };
	D3D11_VIEWPORT viewport = { 0.0f, 0.0f, mirrorTexture->desc.Width, mirrorTexture->desc.Height, D3D11_MIN_DEPTH, D3D11_MIN_DEPTH };
	pContext->RSSetViewports(1, &viewport);
	pContext->ClearRenderTargetView((ID3D11RenderTargetView*)mirrorTexture->target, clear);
	pContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&mirrorTexture->target, NULL);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->Draw(4, 0);

	HRESULT hr = texture->QueryInterface(iid, out_Buffer);
	if (FAILED(hr))
		return ovrError_InvalidParameter;

	// Clean up and return
	mirrorViews[0]->Release();
	mirrorViews[1]->Release();
	pDevice->Release();
	pContext->Release();
	return ovrSuccess;
}
