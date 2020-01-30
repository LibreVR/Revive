#include "OVR_CAPI_D3D.h"
#include "Common.h"
#include "Session.h"
#include "SwapChain.h"
#include "InputManager.h"
#include "XR_Math.h"

#include <detours.h>
#include <vector>
#include <algorithm>

#define XR_USE_GRAPHICS_API_D3D11
#include <d3d11.h>
#include <openxr/openxr_platform.h>

LONG DetourVirtual(PVOID pInstance, UINT methodPos, PVOID *ppPointer, PVOID pDetour)
{
	if (!pInstance || !ppPointer)
		return ERROR_INVALID_PARAMETER;

	LPVOID* pVMT = *((LPVOID**)pInstance);
	*ppPointer = pVMT[methodPos];
	return DetourAttach(ppPointer, pDetour);
}

typedef HRESULT(WINAPI* _CreateRenderTargetView)(
	ID3D11Device						*pDevice,
	ID3D11Resource                      *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView              **ppSRView
	);

_CreateRenderTargetView TrueCreateRenderTargetView;

// {EBA3BA6A-76A2-4A9B-B150-681FC1020EDE}
static const GUID RXR_RTV_DESC =
{ 0xeba3ba6a, 0x76a2, 0x4a9b,{ 0xb1, 0x50, 0x68, 0x1f, 0xc1, 0x2, 0xe, 0xde } };

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
		case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:  return DXGI_FORMAT_R8G8B8A8_UNORM;
		case OVR_FORMAT_B8G8R8A8_UNORM:       return DXGI_FORMAT_B8G8R8A8_UNORM;
		case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:  return DXGI_FORMAT_B8G8R8A8_UNORM;
		case OVR_FORMAT_B8G8R8X8_UNORM:       return DXGI_FORMAT_R8G8B8A8_UNORM;
		case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return DXGI_FORMAT_B8G8R8A8_UNORM;
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

D3D11_RTV_DIMENSION DescToViewDimension(const ovrTextureSwapChainDesc* desc)
{
	if (desc->ArraySize > 1)
	{
		if (desc->SampleCount > 1)
			return D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		else
			return D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	}
	else
	{
		if (desc->SampleCount > 1)
			return D3D11_RTV_DIMENSION_TEXTURE2DMS;
		else
			return D3D11_RTV_DIMENSION_TEXTURE2D;
	}
}

HRESULT WINAPI HookCreateRenderTargetView(
	ID3D11Device						*pDevice,
	ID3D11Resource                      *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView              **ppSRView
)
{
	D3D11_RENDER_TARGET_VIEW_DESC desc;
	UINT size = (UINT)sizeof(desc);
	if (!pDesc && SUCCEEDED(pResource->GetPrivateData(RXR_RTV_DESC, &size, &desc)))
	{
		return TrueCreateRenderTargetView(pDevice, pResource, &desc, ppSRView);
	}

	return TrueCreateRenderTargetView(pDevice, pResource, pDesc, ppSRView);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainDX(ovrSession session,
                                                            IUnknown* d3dPtr,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	REV_TRACE(ovr_CreateTextureSwapChainDX);

	if (!session)
		return ovrError_InvalidSession;

	if (!d3dPtr || !desc || !out_TextureSwapChain || desc->Type != ovrTexture_2D)
		return ovrError_InvalidParameter;

	if (!session->Session)
	{
		ID3D11Device* pDevice = nullptr;
		HRESULT hr = d3dPtr->QueryInterface(&pDevice);
		if (FAILED(hr))
			return ovrError_InvalidParameter;

		// Install a hook on CreateRenderTargetView so we can ensure NULL descriptors keep working on typeless formats
		// This fixes Echo Arena.
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourVirtual(pDevice, 9, (PVOID*)&TrueCreateRenderTargetView, HookCreateRenderTargetView);
		DetourTransactionCommit();
		session->HookedFunction = std::make_pair((PVOID*)&TrueCreateRenderTargetView, HookCreateRenderTargetView);

		XrGraphicsBindingD3D11KHR graphicsBinding = XR_TYPE(GRAPHICS_BINDING_D3D11_KHR);
		graphicsBinding.device = pDevice;

		XrSessionCreateInfo createInfo = XR_TYPE(SESSION_CREATE_INFO);
		createInfo.next = &graphicsBinding;
		createInfo.systemId = session->System;
		CHK_XR(xrCreateSession(session->Instance, &createInfo, &session->Session));

		// Attach it to the InputManager
		session->Input->AttachSession(session->Session);

		// Create reference spaces
		XrReferenceSpaceCreateInfo spaceInfo = XR_TYPE(REFERENCE_SPACE_CREATE_INFO);
		spaceInfo.poseInReferenceSpace = XR::Posef::Identity();
		spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
		CHK_XR(xrCreateReferenceSpace(session->Session, &spaceInfo, &session->ViewSpace));
		spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
		CHK_XR(xrCreateReferenceSpace(session->Session, &spaceInfo, &session->LocalSpace));
		spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		CHK_XR(xrCreateReferenceSpace(session->Session, &spaceInfo, &session->StageSpace));
		session->CalibratedOrigin = OVR::Posef::Identity();

		XrSessionBeginInfo beginInfo = XR_TYPE(SESSION_BEGIN_INFO);
		beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		CHK_XR(xrBeginSession(session->Session, &beginInfo));

		CHK_OVR(ovr_WaitToBeginFrame(session, 0));
		CHK_OVR(ovr_BeginFrame(session, 0));
	}

	// Enumerate formats
	uint32_t formatCount = 0;
	xrEnumerateSwapchainFormats(session->Session, 0, &formatCount, nullptr);
	std::vector<int64_t> formats;
	formats.resize(formatCount);
	xrEnumerateSwapchainFormats(session->Session, (uint32_t)formats.size(), &formatCount, formats.data());
	assert(formats.size() == formatCount);

	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData();

	// Check if the format is supported
	swapChain->Format = TextureFormatToDXGIFormat(desc->Format);

	if (std::find(formats.begin(), formats.end(), swapChain->Format) == formats.end()) {
		swapChain->Format = formats[0];
		if (session->Details->UseHack(SessionDetails::HACK_WMR_SRGB))
			swapChain->Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	swapChain->Desc = *desc;
	XrSwapchainCreateInfo createInfo = DescToCreateInfo(desc, swapChain->Format);
	CHK_XR(xrCreateSwapchain(session->Session, &createInfo, &swapChain->Swapchain));

	CHK_XR(xrEnumerateSwapchainImages(swapChain->Swapchain, 0, &swapChain->Length, nullptr));
	XrSwapchainImageD3D11KHR* images = new XrSwapchainImageD3D11KHR[swapChain->Length];
	for (uint32_t i = 0; i < swapChain->Length; i++)
		images[i] = XR_TYPE(SWAPCHAIN_IMAGE_D3D11_KHR);
	swapChain->Images = (XrSwapchainImageBaseHeader*)images;

	uint32_t finalLength;
	CHK_XR(xrEnumerateSwapchainImages(swapChain->Swapchain, swapChain->Length, &finalLength, swapChain->Images));
	assert(swapChain->Length == finalLength);

	XrSwapchainImageAcquireInfo acqInfo = XR_TYPE(SWAPCHAIN_IMAGE_ACQUIRE_INFO);
	CHK_XR(xrAcquireSwapchainImage(swapChain->Swapchain, &acqInfo, &swapChain->CurrentIndex));

	XrSwapchainImageWaitInfo waitInfo = XR_TYPE(SWAPCHAIN_IMAGE_WAIT_INFO);
	waitInfo.timeout = XR_NO_DURATION;
	CHK_XR(xrWaitSwapchainImage(swapChain->Swapchain, &waitInfo));

	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
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

	D3D11_RENDER_TARGET_VIEW_DESC desc;
	// Under WMR we should always render sRGB even when the swapchain
	// format is linear to compensate for mishandled gamma in the runtime
	if (session->Details->UseHack(SessionDetails::HACK_WMR_SRGB)
	    && chain->Format == DXGI_FORMAT_R8G8B8A8_UNORM)
		desc.Format = (DXGI_FORMAT) DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	else
		desc.Format = (DXGI_FORMAT) chain->Format;
	desc.ViewDimension = DescToViewDimension(&chain->Desc);
	if (desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
	{
		desc.Texture2DMSArray.FirstArraySlice = 0;
		desc.Texture2DMSArray.ArraySize = chain->Desc.ArraySize;
	}
	else
	{
		desc.Texture2DArray.MipSlice = 0;
		desc.Texture2DArray.FirstArraySlice = 0;
		desc.Texture2DArray.ArraySize = chain->Desc.ArraySize;
	}
	HRESULT hr = image.texture->SetPrivateData(RXR_RTV_DESC, sizeof(D3D11_RENDER_TARGET_VIEW_DESC), &desc);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	hr = image.texture->QueryInterface(iid, out_Buffer);
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
