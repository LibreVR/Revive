#include "OVR_CAPI_D3D.h"

#include "openvr.h"
#include <d3d11.h>

#include "REV_Assert.h"
#include "REV_Common.h"

#include "MirrorVS.hlsl.h"
#include "MirrorPS.hlsl.h"

typedef std::pair<ID3D11VertexShader*, ID3D11PixelShader*> ShaderProgram;

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

			// Added in 1.5 compressed formats can be used for static layers
			case OVR_FORMAT_BC1_UNORM:				return DXGI_FORMAT_BC1_TYPELESS;
			case OVR_FORMAT_BC1_UNORM_SRGB:			return DXGI_FORMAT_BC1_TYPELESS;
			case OVR_FORMAT_BC2_UNORM:				return DXGI_FORMAT_BC2_TYPELESS;
			case OVR_FORMAT_BC2_UNORM_SRGB:			return DXGI_FORMAT_BC2_TYPELESS;
			case OVR_FORMAT_BC3_UNORM:				return DXGI_FORMAT_BC3_TYPELESS;
			case OVR_FORMAT_BC3_UNORM_SRGB:			return DXGI_FORMAT_BC3_TYPELESS;
			case OVR_FORMAT_BC6H_UF16:				return DXGI_FORMAT_BC6H_TYPELESS;
			case OVR_FORMAT_BC6H_SF16:				return DXGI_FORMAT_BC6H_TYPELESS;
			case OVR_FORMAT_BC7_UNORM:				return DXGI_FORMAT_BC7_TYPELESS;
			case OVR_FORMAT_BC7_UNORM_SRGB:			return DXGI_FORMAT_BC7_TYPELESS;

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

			// Added in 1.5 compressed formats can be used for static layers
			case OVR_FORMAT_BC1_UNORM:				return DXGI_FORMAT_BC1_UNORM;
			case OVR_FORMAT_BC1_UNORM_SRGB:			return DXGI_FORMAT_BC1_UNORM_SRGB;
			case OVR_FORMAT_BC2_UNORM:				return DXGI_FORMAT_BC2_UNORM;
			case OVR_FORMAT_BC2_UNORM_SRGB:			return DXGI_FORMAT_BC2_UNORM_SRGB;
			case OVR_FORMAT_BC3_UNORM:				return DXGI_FORMAT_BC3_UNORM;
			case OVR_FORMAT_BC3_UNORM_SRGB:			return DXGI_FORMAT_BC3_UNORM_SRGB;
			case OVR_FORMAT_BC6H_UF16:				return DXGI_FORMAT_BC6H_UF16;
			case OVR_FORMAT_BC6H_SF16:				return DXGI_FORMAT_BC6H_SF16;
			case OVR_FORMAT_BC7_UNORM:				return DXGI_FORMAT_BC7_UNORM;
			case OVR_FORMAT_BC7_UNORM_SRGB:			return DXGI_FORMAT_BC7_UNORM_SRGB;

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

UINT ovr_BindFlagsToD3DMiscFlags(unsigned int flags)
{
	UINT result = 0;
	if (flags & ovrTextureBind_DX_RenderTarget)
		result |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
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
	tdesc.MiscFlags = ovr_BindFlagsToD3DMiscFlags(desc->BindFlags);

	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData(vr::API_DirectX, *desc);

	for (int i = 0; i < swapChain->Length; i++)
	{
		ID3D11Texture2D* texture;
		swapChain->Textures[i].eType = vr::API_DirectX;
		swapChain->Textures[i].eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
		hr = pDevice->CreateTexture2D(&tdesc, nullptr, &texture);
		if (FAILED(hr))
			return ovrError_RuntimeException;

		swapChain->Textures[i].handle = texture;
	}

	// Clean up and return
	pDevice->Release();
	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

void rev_DestroyTextureSwapChainDX(ovrTextureSwapChain chain)
{
	for (int i = 0; i < chain->Length; i++)
		((ID3D11Texture2D*)chain->Textures[i].handle)->Release();
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

	ID3D11Texture2D* texturePtr = (ID3D11Texture2D*)chain->Textures[index].handle;
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
	tdesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	hr = pDevice->CreateTexture2D(&tdesc, nullptr, &texture);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData(vr::API_DirectX, *desc);
	mirrorTexture->Texture.handle = texture;
	mirrorTexture->Texture.eType = vr::API_DirectX;
	mirrorTexture->Texture.eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.

	// Create a render target for the mirror texture.
	hr = pDevice->CreateRenderTargetView(texture, NULL, (ID3D11RenderTargetView**)&mirrorTexture->Target);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	// Compile the blitting shaders.
	ID3D11VertexShader* vs;
	hr = pDevice->CreateVertexShader(g_MirrorVS, sizeof(g_MirrorVS), NULL, &vs);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	ID3D11PixelShader* ps;
	hr = pDevice->CreatePixelShader(g_MirrorPS, sizeof(g_MirrorPS), NULL, &ps);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	mirrorTexture->Shader = new ShaderProgram(vs, ps);

	// Get the mirror textures
	vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Left, pDevice, &mirrorTexture->Views[ovrEye_Left]);
	vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Right, pDevice, &mirrorTexture->Views[ovrEye_Right]);

	// Clean up and return
	pDevice->Release();
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

void rev_DestroyMirrorTextureDX(ovrMirrorTexture mirrorTexture)
{
	((ID3D11Texture2D*)mirrorTexture->Texture.handle)->Release();
	((ID3D11RenderTargetView*)mirrorTexture->Target)->Release();
	((ID3D11ShaderResourceView*)mirrorTexture->Views[ovrEye_Left])->Release();
	((ID3D11ShaderResourceView*)mirrorTexture->Views[ovrEye_Right])->Release();

	ShaderProgram* shader = (ShaderProgram*)mirrorTexture->Shader;
	shader->first->Release();
	shader->second->Release();
	delete shader;
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

	ID3D11Texture2D* texture = (ID3D11Texture2D*)mirrorTexture->Texture.handle;

	ID3D11Device* pDevice;
	texture->GetDevice(&pDevice);
	ID3D11DeviceContext* pContext;
	pDevice->GetImmediateContext(&pContext);

	// Compile the shaders if it is not yet set
	ShaderProgram* shader = (ShaderProgram*)mirrorTexture->Shader;
	pContext->VSSetShader(shader->first, NULL, 0);
	pContext->PSSetShader(shader->second, NULL, 0);
	pContext->PSSetShaderResources(0, 2, (ID3D11ShaderResourceView**)mirrorTexture->Views);

	// Draw a triangle strip, the vertex buffer is not used.
	FLOAT clear[4] = { 0.0f };
	D3D11_VIEWPORT viewport = { 0.0f, 0.0f, mirrorTexture->Desc.Width, mirrorTexture->Desc.Height, D3D11_MIN_DEPTH, D3D11_MIN_DEPTH };
	pContext->RSSetViewports(1, &viewport);
	pContext->ClearRenderTargetView((ID3D11RenderTargetView*)mirrorTexture->Target, clear);
	pContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&mirrorTexture->Target, NULL);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->Draw(4, 0);

	HRESULT hr = texture->QueryInterface(iid, out_Buffer);
	if (FAILED(hr))
		return ovrError_InvalidParameter;

	// Clean up and return
	pDevice->Release();
	pContext->Release();
	return ovrSuccess;
}
