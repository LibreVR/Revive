#include "OVR_CAPI_D3D.h"
#include "Session.h"
#include "CompositorD3D.h"
#include "TextureD3D.h"

#include <d3d11.h>
#include <dxgi1_3.h>

#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
using namespace Windows::Graphics::DirectX::Direct3D11;

#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainDX(ovrSession session,
                                                            IUnknown* d3dPtr,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	if (!session)
		return ovrError_InvalidSession;

	if (!d3dPtr || !desc || !out_TextureSwapChain || desc->Type != ovrTexture_2D)
		return ovrError_InvalidParameter;

	// TODO: DX12 support
	if (!session->Compositor)
	{
		IDirect3DDevice device;
		winrt::com_ptr<IDXGIDevice3> pDevice;
		HRESULT hr = d3dPtr->QueryInterface(pDevice.put());
		if (SUCCEEDED(hr))
		{
			winrt::com_ptr<IInspectable> inspectableDevice;
			hr = CreateDirect3D11DeviceFromDXGIDevice(pDevice.get(), inspectableDevice.put());
			device = inspectableDevice.as<IDirect3DDevice>();
		}
		if (SUCCEEDED(hr))
		{
			try
			{
				session->Space.SetDirect3D11Device(device);
				session->Frame = session->Space.CreateNextFrame();
			}
			catch (winrt::hresult_invalid_argument& ex)
			{
				OutputDebugStringW(ex.message().c_str());
				OutputDebugStringW(L"\n");
				return ovrError_RuntimeException;
			}
		}

		session->Compositor.reset(CompositorD3D::Create(d3dPtr));
		if (!session->Compositor)
			return ovrError_RuntimeException;
	}

	return session->Compositor->CreateTextureSwapChain(desc, out_TextureSwapChain);
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

	if (index < 0)
		index = chain->CurrentIndex;

	TextureD3D* texture = dynamic_cast<TextureD3D*>(chain->Textures[index].get());
	if (!texture)
		return ovrError_InvalidParameter;

	HRESULT hr = texture->Texture()->QueryInterface(iid, out_Buffer);
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

	if (!session->Compositor)
	{
		session->Compositor.reset(CompositorD3D::Create(d3dPtr));
		if (!session->Compositor)
			return ovrError_RuntimeException;
	}

	return session->Compositor->CreateMirrorTexture(desc, out_MirrorTexture);
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
	if (!session)
		return ovrError_InvalidSession;

	if (!mirrorTexture || !out_Buffer)
		return ovrError_InvalidParameter;

	TextureD3D* texture = dynamic_cast<TextureD3D*>(mirrorTexture->Texture.get());
	if (!texture)
		return ovrError_InvalidParameter;

	HRESULT hr = texture->Texture()->QueryInterface(iid, out_Buffer);
	if (FAILED(hr))
		return ovrError_InvalidParameter;

	return ovrSuccess;
}
