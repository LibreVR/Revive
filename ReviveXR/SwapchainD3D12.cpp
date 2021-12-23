#include "SwapchainD3D12.h"
#include "Common.h"
#include "Session.h"

#define XR_USE_GRAPHICS_API_D3D12
#include <d3d12.h>
#include <openxr/openxr_platform.h>

ovrResult ovrTextureSwapChainD3D12::Create(ovrSession session, const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	// Do some format compatibility conversions before creating the swapchain
	DXGI_FORMAT format = NegotiateFormat(session, TextureFormatToDXGIFormat(desc->Format));
	assert(session->SupportsFormat(format));

	ovrTextureSwapChain chain = new ovrTextureSwapChainD3D12();
	CHK_OVR(chain->Init(session->Session, desc, TextureFormatToDXGIFormat(desc->Format)));
	CHK_OVR(EnumerateImages<XrSwapchainImageD3D12KHR>(XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR, chain));

	// If the app doesn't expect a typeless texture we need to attach the fully qualified format to each texture.
	// Depth formats are always considered typeless in the Oculus SDK, so no need to hook those.
	if (!(desc->MiscFlags & ovrTextureMisc_DX_Typeless) && !IsDepthFormat(desc->Format))
	{
		// Create the SRVs and RTVs with the real format that the application expects
		D3D12_SHADER_RESOURCE_VIEW_DESC srv = { format, (D3D12_SRV_DIMENSION)DescToViewDimension(&chain->Desc), D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING };
		D3D12_RENDER_TARGET_VIEW_DESC rtv = { format, (D3D12_RTV_DIMENSION)DescToViewDimension(&chain->Desc) };
		srv.Texture2D.MipLevels = chain->Desc.MipLevels;
		if (chain->Desc.ArraySize > 1)
		{
			srv.Texture2DArray.ArraySize = chain->Desc.ArraySize;
			rtv.Texture2DArray.ArraySize = chain->Desc.ArraySize;
		}

		for (uint32_t i = 0; i < chain->Length; i++)
		{
			XrSwapchainImageD3D12KHR image = ((XrSwapchainImageD3D12KHR*)chain->Images)[i];

			HRESULT hr = image.texture->SetPrivateData(RXR_SRV_DESC, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC), &srv);
			if (SUCCEEDED(hr))
				hr = image.texture->SetPrivateData(RXR_RTV_DESC, sizeof(D3D12_RENDER_TARGET_VIEW_DESC), &rtv);
			if (FAILED(hr))
				return ovrError_RuntimeException;
		}
	}

	*out_TextureSwapChain = chain;
	return ovrSuccess;
}
