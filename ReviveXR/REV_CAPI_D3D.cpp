#include "OVR_CAPI_D3D.h"
#include "Common.h"
#include "Session.h"
#include "SwapChain.h"
#include "InputManager.h"
#include "XR_Math.h"

#define XR_USE_GRAPHICS_API_D3D11
#include <d3d11.h>
#include <openxr/openxr_platform.h>

DXGI_FORMAT TextureFormatToDXGIFormat(ovrTextureFormat format)
{
	// TODO: sRGB support
	// TODO: Typeless formats are unsupported, we may need to introduce a typeless staging texture
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
		case OVR_FORMAT_B8G8R8X8_UNORM:       return DXGI_FORMAT_B8G8R8X8_UNORM;
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

		XrGraphicsBindingD3D11KHR graphicsBinding = XR_TYPE(GRAPHICS_BINDING_D3D11_KHR);
		graphicsBinding.device = pDevice;

		XrSessionCreateInfo createInfo = XR_TYPE(SESSION_CREATE_INFO);
		createInfo.next = &graphicsBinding;
		createInfo.systemId = session->System;
		CHK_XR(xrCreateSession(session->Instance, &createInfo, &session->Session));

		// Create reference spaces
		XrReferenceSpaceCreateInfo spaceInfo = XR_TYPE(REFERENCE_SPACE_CREATE_INFO);
		spaceInfo.poseInReferenceSpace = XR::Posef(XR::Posef::Identity());
		spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
		CHK_XR(xrCreateReferenceSpace(session->Session, &spaceInfo, &session->ViewSpace));
		spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
		CHK_XR(xrCreateReferenceSpace(session->Session, &spaceInfo, &session->LocalSpace));
		spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		CHK_XR(xrCreateReferenceSpace(session->Session, &spaceInfo, &session->StageSpace));

		// Initialize input
		session->Input.reset(new InputManager(session));

		XrSessionBeginInfo beginInfo = XR_TYPE(SESSION_BEGIN_INFO);
		beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		CHK_XR(xrBeginSession(session->Session, &beginInfo));

		CHK_OVR(ovr_WaitToBeginFrame(session, 0));
		CHK_OVR(ovr_BeginFrame(session, 0));
	}

	XrSwapchainCreateInfo createInfo = DescToCreateInfo(desc, TextureFormatToDXGIFormat(desc->Format));
	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData();
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

	return ovrError_Unsupported;
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

	return ovrError_Unsupported;
}
