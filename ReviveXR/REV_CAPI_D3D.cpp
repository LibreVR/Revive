#include "OVR_CAPI_D3D.h"
#include "Common.h"
#include "Session.h"
#include "Runtime.h"
#include "SwapChain.h"
#include "XR_Math.h"

#include <detours/detours.h>
#include <vector>
#include <algorithm>

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#include <d3d11.h>
#include <d3d12.h>
#include <openxr/openxr_platform.h>

LONG DetourVirtual(PVOID pInstance, UINT methodPos, PVOID *ppPointer, PVOID pDetour)
{
	if (!pInstance || !ppPointer)
		return ERROR_INVALID_PARAMETER;

	LPVOID* pVMT = *((LPVOID**)pInstance);
	*ppPointer = pVMT[methodPos];
	return DetourAttach(ppPointer, pDetour);
}

// {EBA3BA6A-76A2-4A9B-B150-681FC1020EDE}
static const GUID RXR_RTV_DESC =
{ 0xeba3ba6a, 0x76a2, 0x4a9b,{ 0xb1, 0x50, 0x68, 0x1f, 0xc1, 0x2, 0xe, 0xde } };
// {FD3C4A2A-F328-4CC7-9495-A96CF78DEE46}
static const GUID RXR_SRV_DESC =
{ 0xfd3c4a2a, 0xf328, 0x4cc7, { 0x94, 0x95, 0xa9, 0x6c, 0xf7, 0x8d, 0xee, 0x46 } };

typedef HRESULT(WINAPI* _CreateTexture2D)(
	ID3D11Device                 *This,
	const D3D11_TEXTURE2D_DESC   *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
	ID3D11Texture2D              **ppTexture2D);
typedef HRESULT(WINAPI* _CreateShaderResourceView)(
	ID3D11Device						  *This,
	ID3D11Resource                        *pResource,
	const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
	ID3D11ShaderResourceView              **ppSRView);
typedef HRESULT(WINAPI* _CreateRenderTargetView)(
	ID3D11Device						*This,
	ID3D11Resource                      *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView              **ppRTView);

_CreateTexture2D TrueCreateTexture2D;
_CreateShaderResourceView TrueCreateShaderResourceView;
_CreateRenderTargetView TrueCreateRenderTargetView;

thread_local const ovrTextureSwapChainDesc* g_SwapChainDesc = nullptr;
HRESULT WINAPI HookCreateTexture2D(
	ID3D11Device                 *This,
	const D3D11_TEXTURE2D_DESC   *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
	ID3D11Texture2D              **ppTexture2D)
{
	if (g_SwapChainDesc)
	{
		D3D11_TEXTURE2D_DESC desc = *pDesc;

		// Force support for 8-bit linear formats
		if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
			g_SwapChainDesc->Format == OVR_FORMAT_R8G8B8A8_UNORM)
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		else if (desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB &&
			g_SwapChainDesc->Format == OVR_FORMAT_B8G8R8A8_UNORM)
			desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

		// Force support for swapchain mipmaps
		desc.MipLevels = g_SwapChainDesc->MipLevels;
		if (desc.MipLevels > 1)
			desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

		// Force support for XR_SWAPCHAIN_USAGE_SAMPLED_BIT on all formats
		desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		return TrueCreateTexture2D(This, &desc, pInitialData, ppTexture2D);
	}

	return TrueCreateTexture2D(This, pDesc, pInitialData, ppTexture2D);
}

HRESULT WINAPI HookCreateRenderTargetView(
	ID3D11Device						*This,
	ID3D11Resource                      *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView              **ppRTView)
{
	D3D11_RENDER_TARGET_VIEW_DESC desc;
	UINT size = (UINT)sizeof(desc);
	if (SUCCEEDED(pResource->GetPrivateData(RXR_RTV_DESC, &size, &desc)))
	{
		return TrueCreateRenderTargetView(This, pResource, &desc, ppRTView);
	}

	return TrueCreateRenderTargetView(This, pResource, pDesc, ppRTView);
}

HRESULT WINAPI HookCreateShaderResourceView(
	ID3D11Device						  *This,
	ID3D11Resource                        *pResource,
	const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
	ID3D11ShaderResourceView              **ppSRView)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	UINT size = (UINT)sizeof(desc);
	if (SUCCEEDED(pResource->GetPrivateData(RXR_SRV_DESC, &size, &desc)))
	{
		return TrueCreateShaderResourceView(This, pResource, &desc, ppSRView);
	}

	return TrueCreateShaderResourceView(This, pResource, pDesc, ppSRView);
}

DXGI_FORMAT TextureFormatToDXGIFormat(ovrTextureFormat format)
{
	// TODO: sRGB support
	// OpenXR always uses typeless formats
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
		case OVR_FORMAT_B8G8R8X8_UNORM:       return DXGI_FORMAT_R8G8B8A8_UNORM;
		case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		case OVR_FORMAT_R16G16B16A16_FLOAT:   return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case OVR_FORMAT_R11G11B10_FLOAT:      return DXGI_FORMAT_R11G11B10_FLOAT;

		// Depth formats
		case OVR_FORMAT_D16_UNORM:            return DXGI_FORMAT_D16_UNORM;
		case OVR_FORMAT_D24_UNORM_S8_UINT:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case OVR_FORMAT_D32_FLOAT:            return DXGI_FORMAT_D32_FLOAT;
		case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

		// Added in 1.5 compressed formats can be used for static layers
		case OVR_FORMAT_BC1_UNORM:            return DXGI_FORMAT_BC1_UNORM;
		case OVR_FORMAT_BC1_UNORM_SRGB:       return DXGI_FORMAT_BC1_UNORM;
		case OVR_FORMAT_BC2_UNORM:            return DXGI_FORMAT_BC2_UNORM;
		case OVR_FORMAT_BC2_UNORM_SRGB:       return DXGI_FORMAT_BC2_UNORM;
		case OVR_FORMAT_BC3_UNORM:            return DXGI_FORMAT_BC3_UNORM;
		case OVR_FORMAT_BC3_UNORM_SRGB:       return DXGI_FORMAT_BC3_UNORM;
		case OVR_FORMAT_BC6H_UF16:            return DXGI_FORMAT_BC6H_UF16;
		case OVR_FORMAT_BC6H_SF16:            return DXGI_FORMAT_BC6H_SF16;
		case OVR_FORMAT_BC7_UNORM:            return DXGI_FORMAT_BC7_UNORM;
		case OVR_FORMAT_BC7_UNORM_SRGB:       return DXGI_FORMAT_BC7_UNORM;

		default: return DXGI_FORMAT_UNKNOWN;
	}
}

D3D_SRV_DIMENSION DescToViewDimension(const ovrTextureSwapChainDesc* desc)
{
	if (desc->ArraySize > 1)
	{
		if (desc->SampleCount > 1)
			return D3D_SRV_DIMENSION_TEXTURE2DMSARRAY;
		else
			return D3D_SRV_DIMENSION_TEXTURE2DARRAY;
	}
	else
	{
		if (desc->SampleCount > 1)
			return D3D_SRV_DIMENSION_TEXTURE2DMS;
		else
			return D3D_SRV_DIMENSION_TEXTURE2D;
	}
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainDX(ovrSession session,
                                                            IUnknown* d3dPtr,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	REV_TRACE(ovr_CreateTextureSwapChainDX);

	if (!session)
		return ovrError_InvalidSession;

	if (!d3dPtr || !desc || !out_TextureSwapChain || desc->Type == ovrTexture_2D_External)
		return ovrError_InvalidParameter;

	if (desc->Type == ovrTexture_Cube && !Runtime::Get().CompositionCube)
		return ovrError_Unsupported;

	ID3D11Device* pDevice = nullptr;
	ID3D12CommandQueue* pQueue = nullptr;
	if (FAILED(d3dPtr->QueryInterface(&pDevice)))
	{
		if (FAILED(d3dPtr->QueryInterface(&pQueue)))
			return ovrError_InvalidParameter;
	}

	if (!session->Session)
	{
		if (pDevice)
		{
			XR_FUNCTION(session->Instance, GetD3D11GraphicsRequirementsKHR);
			XrGraphicsRequirementsD3D11KHR graphicsReq = XR_TYPE(GRAPHICS_REQUIREMENTS_D3D11_KHR);
			CHK_XR(GetD3D11GraphicsRequirementsKHR(session->Instance, session->System, &graphicsReq));

			if (pDevice->GetFeatureLevel() < graphicsReq.minFeatureLevel)
				return ovrError_IncompatibleGPU;

			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			if (Runtime::Get().UseHack(Runtime::HACK_HOOK_CREATE_TEXTURE))
			{
				DetourVirtual(pDevice, 5, (PVOID*)&TrueCreateTexture2D, HookCreateTexture2D);
				session->HookedFunctions.insert(std::make_pair((PVOID*)&TrueCreateTexture2D, HookCreateTexture2D));
			}
			// Install a hook D3D11 functions so we can ensure NULL descriptors keep working on typeless formats
			// This fixes Echo Arena, among others. May need to port this hack to D3D12 in the future.
			DetourVirtual(pDevice, 7, (PVOID*)&TrueCreateShaderResourceView, HookCreateShaderResourceView);
			session->HookedFunctions.insert(std::make_pair((PVOID*)&TrueCreateShaderResourceView, HookCreateShaderResourceView));
			DetourVirtual(pDevice, 9, (PVOID*)&TrueCreateRenderTargetView, HookCreateRenderTargetView);
			session->HookedFunctions.insert(std::make_pair((PVOID*)&TrueCreateRenderTargetView, HookCreateRenderTargetView));
			DetourTransactionCommit();

			XrGraphicsBindingD3D11KHR graphicsBinding = XR_TYPE(GRAPHICS_BINDING_D3D11_KHR);
			graphicsBinding.device = pDevice;
			session->BeginSession(&graphicsBinding);
		}
		else if (pQueue)
		{
			if (!Runtime::Get().Supports(XR_KHR_D3D12_ENABLE_EXTENSION_NAME))
				return ovrError_Unsupported;

			ID3D12Device* pDevice12 = nullptr;
			if (FAILED(pQueue->GetDevice(__uuidof(*pDevice12), reinterpret_cast<void**>(&pDevice12))))
				return ovrError_RuntimeException;

			XR_FUNCTION(session->Instance, GetD3D12GraphicsRequirementsKHR);
			XrGraphicsRequirementsD3D12KHR graphicsReq = XR_TYPE(GRAPHICS_REQUIREMENTS_D3D12_KHR);
			CHK_XR(GetD3D12GraphicsRequirementsKHR(session->Instance, session->System, &graphicsReq));

			static const D3D_FEATURE_LEVEL featureLevels[] = { graphicsReq.minFeatureLevel };
			D3D12_FEATURE_DATA_FEATURE_LEVELS levels = { 1, featureLevels };
			pDevice12->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels));

			if (levels.MaxSupportedFeatureLevel < graphicsReq.minFeatureLevel)
				return ovrError_IncompatibleGPU;

			XrGraphicsBindingD3D12KHR graphicsBinding = XR_TYPE(GRAPHICS_BINDING_D3D12_KHR);
			graphicsBinding.device = pDevice12;
			graphicsBinding.queue = pQueue;
			session->BeginSession(&graphicsBinding);
		}
		else
		{
			return ovrError_RuntimeException;
		}
	}

	DXGI_FORMAT format = TextureFormatToDXGIFormat(desc->Format);
	if (format == DXGI_FORMAT_R11G11B10_FLOAT)
	{
		if (Runtime::Get().UseHack(Runtime::HACK_NO_11BIT_FORMAT))
			format = DXGI_FORMAT_R10G10B10A2_UNORM;
		else if (Runtime::Get().UseHack(Runtime::HACK_NO_10BIT_FORMAT))
			format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	}
	else if ((format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM) &&
		Runtime::Get().UseHack(Runtime::HACK_NO_8BIT_LINEAR))
	{
		if (format == DXGI_FORMAT_R8G8B8A8_UNORM)
			format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		else
			format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	}

	if (pDevice)
	{
		ovrTextureSwapChain chain;
		g_SwapChainDesc = desc;
		CHK_OVR(CreateSwapChain(session->Session, desc, format, &chain));
		g_SwapChainDesc = nullptr;
		CHK_OVR(EnumerateImages<XrSwapchainImageD3D11KHR>(XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR, chain));

		// If the app doesn't expect a typeless texture we need to attach the fully qualified format to each texture.
		if (!(desc->MiscFlags & ovrTextureMisc_DX_Typeless))
		{
			CD3D11_SHADER_RESOURCE_VIEW_DESC srv(DescToViewDimension(&chain->Desc), format);
			CD3D11_RENDER_TARGET_VIEW_DESC rtv((D3D11_RTV_DIMENSION)DescToViewDimension(&chain->Desc), format);

			for (uint32_t i = 0; i < chain->Length; i++)
			{
				XrSwapchainImageD3D11KHR image = ((XrSwapchainImageD3D11KHR*)chain->Images)[i];

				HRESULT hr = image.texture->SetPrivateData(RXR_SRV_DESC, sizeof(CD3D11_SHADER_RESOURCE_VIEW_DESC), &srv);
				if (SUCCEEDED(hr))
					hr = image.texture->SetPrivateData(RXR_RTV_DESC, sizeof(D3D11_RENDER_TARGET_VIEW_DESC), &rtv);
				if (FAILED(hr))
					return ovrError_RuntimeException;
			}
		}

		*out_TextureSwapChain = chain;
		return ovrSuccess;
	}
	else if (pQueue)
	{
		CHK_OVR(CreateSwapChain(session->Session, desc, format, out_TextureSwapChain));
		return EnumerateImages<XrSwapchainImageD3D12KHR>(XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR, *out_TextureSwapChain);
	}
	else
	{
		return ovrError_RuntimeException;
	}
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainBufferDX(ovrSession session,
                                                               ovrTextureSwapChain chain,
                                                               int index,
                                                               IID iid,
                                                               void** out_Buffer)
{
	REV_TRACE(ovr_GetTextureSwapChainBufferDX);

	if (!session)
		return ovrError_InvalidSession;

	if (!chain || !out_Buffer)
		return ovrError_InvalidParameter;

	if (index < 0)
		index = chain->CurrentIndex;

	XrSwapchainImageD3D11KHR image = ((XrSwapchainImageD3D11KHR*)chain->Images)[index];

	HRESULT hr = image.texture->QueryInterface(iid, out_Buffer);
	if (FAILED(hr))
		return ovrError_InvalidParameter;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureDX(ovrSession session,
                                                         IUnknown* d3dPtr,
                                                         const ovrMirrorTextureDesc* desc,
                                                         ovrMirrorTexture* out_MirrorTexture)
{
	REV_TRACE(ovr_CreateMirrorTextureDX);

	if (!session)
		return ovrError_InvalidSession;

	if (!d3dPtr || !desc || !out_MirrorTexture)
		return ovrError_InvalidParameter;

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData();
	mirrorTexture->Desc = *desc;
	*out_MirrorTexture = mirrorTexture;

	ovrTextureSwapChainDesc swapDesc;
	swapDesc.Type = ovrTexture_2D;
	swapDesc.Format = desc->Format;
	swapDesc.ArraySize = 1;
	swapDesc.Width = desc->Width;
	swapDesc.Height = desc->Height;
	swapDesc.MipLevels = 1;
	swapDesc.SampleCount = 1;
	swapDesc.StaticImage = ovrTrue;
	swapDesc.MiscFlags = desc->MiscFlags;
	swapDesc.BindFlags = 0;
	return ovr_CreateTextureSwapChainDX(session, d3dPtr, &swapDesc, &mirrorTexture->Dummy);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureWithOptionsDX(ovrSession session,
                                                                    IUnknown* d3dPtr,
                                                                    const ovrMirrorTextureDesc* desc,
                                                                    ovrMirrorTexture* out_MirrorTexture)
{
	return ovr_CreateMirrorTextureDX(session, d3dPtr, desc, out_MirrorTexture);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetMirrorTextureBufferDX(ovrSession session,
                                                            ovrMirrorTexture mirrorTexture,
                                                            IID iid,
                                                            void** out_Buffer)
{
	REV_TRACE(ovr_GetMirrorTextureBufferDX);

	if (!session)
		return ovrError_InvalidSession;

	if (!mirrorTexture || !out_Buffer)
		return ovrError_InvalidParameter;

	return ovr_GetTextureSwapChainBufferDX(session, mirrorTexture->Dummy, 0, iid, out_Buffer);
}
