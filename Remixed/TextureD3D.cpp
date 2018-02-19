#include "TextureD3D.h"
#include "OVR_CAPI.h"

#include <d3d11.h>

TextureD3D::TextureD3D(ID3D11Device* pDevice)
	: m_pDevice(pDevice)
{
}

TextureD3D::~TextureD3D()
{
}

DXGI_FORMAT TextureD3D::TextureFormatToDXGIFormat(ovrTextureFormat format, unsigned int flags)
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
			case OVR_FORMAT_R11G11B10_FLOAT:      return DXGI_FORMAT_R10G10B10A2_TYPELESS; // TODO: OpenVR doesn't support R11G11B10

			// Depth formats
			case OVR_FORMAT_D16_UNORM:            return DXGI_FORMAT_R16_TYPELESS;
			case OVR_FORMAT_D24_UNORM_S8_UINT:    return DXGI_FORMAT_R24G8_TYPELESS;
			case OVR_FORMAT_D32_FLOAT:            return DXGI_FORMAT_R32_TYPELESS;
			case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32G32_TYPELESS;

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
			case OVR_FORMAT_R11G11B10_FLOAT:      return DXGI_FORMAT_R10G10B10A2_UNORM; // TODO: OpenVR doesn't support R11G11B10

			// Depth formats
			case OVR_FORMAT_D16_UNORM:            return DXGI_FORMAT_D16_UNORM;
			case OVR_FORMAT_D24_UNORM_S8_UINT:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
			case OVR_FORMAT_D32_FLOAT:            return DXGI_FORMAT_D32_FLOAT;
			case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

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

ovrTextureFormat TextureD3D::ToLinearFormat(ovrTextureFormat format)
{
	switch (format)
	{
		case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:  return OVR_FORMAT_R8G8B8A8_UNORM;
		case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:  return OVR_FORMAT_B8G8R8A8_UNORM;
		case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return OVR_FORMAT_B8G8R8X8_UNORM;

		// Added in 1.5 compressed formats can be used for static layers
		case OVR_FORMAT_BC1_UNORM_SRGB:       return OVR_FORMAT_BC1_UNORM;
		case OVR_FORMAT_BC2_UNORM_SRGB:       return OVR_FORMAT_BC2_UNORM;
		case OVR_FORMAT_BC3_UNORM_SRGB:       return OVR_FORMAT_BC3_UNORM;
		case OVR_FORMAT_BC7_UNORM_SRGB:       return OVR_FORMAT_BC7_UNORM;

		default: return format;
	}
}

UINT TextureD3D::BindFlagsToD3DBindFlags(unsigned int flags)
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

UINT TextureD3D::MiscFlagsToD3DMiscFlags(unsigned int flags)
{
	// TODO: Support ovrTextureMisc_AutoGenerateMips
	UINT result = 0;
	if (flags & ovrTextureMisc_AllowGenerateMips)
		result |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	return result;
}

bool TextureD3D::Init(ovrTextureType type, int Width, int Height, int MipLevels, int ArraySize,
	int SampleCount, ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags)
{
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = Width;
	desc.Height = Height;
	desc.MipLevels = MipLevels;
	desc.ArraySize = ArraySize;
	desc.SampleDesc.Count = SampleCount;
	desc.SampleDesc.Quality = 0;
	desc.Format = TextureFormatToDXGIFormat(Format, MiscFlags);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = BindFlagsToD3DBindFlags(BindFlags);
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = MiscFlagsToD3DMiscFlags(MiscFlags);

	HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, m_pTexture.GetAddressOf());
	if (FAILED(hr))
		return false;

	if (SampleCount <= 1)
	{
		if (MiscFlags & ovrTextureMisc_DX_Typeless)
			Format = ToLinearFormat(Format);
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = TextureFormatToDXGIFormat(Format);
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = -1;
		srv_desc.Texture2D.MostDetailedMip = 0;
		hr = m_pDevice->CreateShaderResourceView(m_pTexture.Get(), &srv_desc, m_pSRV.GetAddressOf());
		if (FAILED(hr))
			return false;
	}

	return true;
}
