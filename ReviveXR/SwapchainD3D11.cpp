#include "SwapchainD3D11.h"
#include "Common.h"
#include "Session.h"

#define XR_USE_GRAPHICS_API_D3D11
#include <d3d11.h>
#include <openxr/openxr_platform.h>

ovrResult ovrTextureSwapChainD3D11::Create(ovrSession session, const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	// Do some format compatibility conversions before creating the swapchain
	DXGI_FORMAT format = NegotiateFormat(session, TextureFormatToDXGIFormat(desc->Format));
	assert(session->SupportsFormat(format));

	ovrTextureSwapChain chain = new ovrTextureSwapChainD3D11();
	CHK_OVR(chain->Init(session->Session, desc, format));
	CHK_OVR(EnumerateImages<XrSwapchainImageD3D11KHR>(XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR, chain));

	// If the app doesn't expect a typeless texture we need to attach the fully qualified format to each texture.
	// Depth formats are always considered typeless in the Oculus SDK, so no need to hook those.
	if (!(desc->MiscFlags & ovrTextureMisc_DX_Typeless) && !IsDepthFormat(desc->Format))
	{
		// Create the SRVs and RTVs with the real format that the application expects
		CD3D11_SHADER_RESOURCE_VIEW_DESC srv(DescToViewDimension(&chain->Desc), format);
		CD3D11_RENDER_TARGET_VIEW_DESC rtv((D3D11_RTV_DIMENSION)DescToViewDimension(&chain->Desc), format);

		for (uint32_t i = 0; i < chain->Length; i++)
		{
			XrSwapchainImageD3D11KHR image = ((XrSwapchainImageD3D11KHR*)chain->Images)[i];

			HRESULT hr = image.texture->SetPrivateData(RXR_SRV_DESC, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC), &srv);
			if (SUCCEEDED(hr))
				hr = image.texture->SetPrivateData(RXR_RTV_DESC, sizeof(D3D11_RENDER_TARGET_VIEW_DESC), &rtv);
			if (FAILED(hr))
				return ovrError_RuntimeException;
		}
	}

	*out_TextureSwapChain = chain;
	return ovrSuccess;
}
