#include "OVR_CAPI_D3D.h"

#include "openvr.h"
#include <d3d11.h>

#include "REV_Assert.h"
#include "REV_Common.h"

DXGI_FORMAT ovr_TextureFormatToDXGIFormat(ovrTextureFormat format)
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
		case OVR_FORMAT_D16_UNORM:				return DXGI_FORMAT_D16_UNORM;
		case OVR_FORMAT_D24_UNORM_S8_UINT:		return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case OVR_FORMAT_D32_FLOAT:				return DXGI_FORMAT_D32_FLOAT;
		case OVR_FORMAT_D32_FLOAT_S8X24_UINT:	return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		default: return DXGI_FORMAT_UNKNOWN;
	}
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainDX(ovrSession session,
                                                            IUnknown* d3dPtr,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	// TODO: DX12 support.
	ID3D11Device* pDevice;
	HRESULT hr = d3dPtr->QueryInterface(&pDevice);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	// TODO: Implement support for texture flags.
	ID3D11Texture2D* texture;
	D3D11_TEXTURE2D_DESC texdesc = { 0 };
	texdesc.Width = desc->Width;
	texdesc.Height = desc->Height;
	texdesc.MipLevels = desc->MipLevels;
	texdesc.ArraySize = desc->ArraySize;
	texdesc.Format = ovr_TextureFormatToDXGIFormat(desc->Format);
	texdesc.Usage = D3D11_USAGE_DEFAULT;
	texdesc.BindFlags = desc->BindFlags;
	pDevice->CreateTexture2D(&texdesc, nullptr, &texture);

	// TODO: Should add multiple buffers to swapchain?
	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData();
	swapChain->length = 1;
	swapChain->index = 0;
	swapChain->desc = *desc;
	texture->QueryInterface(IID_IUnknown, &swapChain->texture.handle);
	swapChain->texture.eType = vr::API_DirectX;
	swapChain->texture.eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainBufferDX(ovrSession session,
                                                               ovrTextureSwapChain chain,
                                                               int index,
                                                               IID iid,
                                                               void** out_Buffer)
{
	IUnknown* texturePtr = (IUnknown*)chain->texture.handle;
	HRESULT hr = texturePtr->QueryInterface(iid, out_Buffer);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureDX(ovrSession session,
                                                         IUnknown* d3dPtr,
                                                         const ovrMirrorTextureDesc* desc,
                                                         ovrMirrorTexture* out_MirrorTexture) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetMirrorTextureBufferDX(ovrSession session,
                                                            ovrMirrorTexture mirrorTexture,
                                                            IID iid,
                                                            void** out_Buffer) { REV_UNIMPLEMENTED_RUNTIME; }
