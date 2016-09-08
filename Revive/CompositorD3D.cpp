#include "CompositorD3D.h"
#include "REV_Common.h"

#include "openvr.h"
#include <d3d11.h>

#include "MirrorVS.hlsl.h"
#include "MirrorPS.hlsl.h"

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

	// Compile the mirror shaders.
	m_pDevice->CreateVertexShader(g_MirrorVS, sizeof(g_MirrorVS), NULL, m_MirrorVS.GetAddressOf());
	m_pDevice->CreatePixelShader(g_MirrorPS, sizeof(g_MirrorPS), NULL, m_MirrorPS.GetAddressOf());
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
		  desc->MiscFlags, desc->BindFlags, (ID3D11Texture2D**)&swapChain->Textures[i].handle);
		if (FAILED(hr))
			return ovrError_RuntimeException;
	}

	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

void CompositorD3D::DestroyTextureSwapChain(ovrTextureSwapChain chain)
{
	for (int i = 0; i < chain->Length; i++)
		((ID3D11Texture2D*)chain->Textures[i].handle)->Release();
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

void CompositorD3D::RenderMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	ID3D11Texture2D* texture = (ID3D11Texture2D*)mirrorTexture->Texture.handle;

	ID3D11Device* pDevice;
	texture->GetDevice(&pDevice);
	ID3D11DeviceContext* pContext;
	pDevice->GetImmediateContext(&pContext);

	// Compile the shaders if it is not yet set
	pContext->VSSetShader(m_MirrorVS.Get(), NULL, 0);
	pContext->PSSetShader(m_MirrorPS.Get(), NULL, 0);
	pContext->PSSetShaderResources(0, 2, (ID3D11ShaderResourceView**)mirrorTexture->Views);

	// Draw a triangle strip, the vertex buffer is not used.
	FLOAT clear[4] = { 0.0f };
	D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)mirrorTexture->Desc.Width, (float)mirrorTexture->Desc.Height, D3D11_MIN_DEPTH, D3D11_MIN_DEPTH };
	pContext->RSSetViewports(1, &viewport);
	pContext->ClearRenderTargetView((ID3D11RenderTargetView*)mirrorTexture->Target, clear);
	pContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&mirrorTexture->Target, NULL);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->Draw(4, 0);

	// Clean up and return
	pDevice->Release();
	pContext->Release();
}
