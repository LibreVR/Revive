#include "CompositorD3D.h"
#include "TextureD3D.h"
#include "Session.h"

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
using namespace Windows::Graphics::DirectX::Direct3D11;

#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

#include <winrt/Windows.Graphics.Holographic.h>
using namespace winrt::Windows::Graphics::Holographic;

struct Vertex
{
	ovrVector2f Position;
	ovrVector2f TexCoord;
};

CompositorD3D* CompositorD3D::Create(IUnknown* d3dPtr)
{
	// Get the device for this context
	// TODO: DX12 support
	ID3D11Device* pDevice = nullptr;
	HRESULT hr = d3dPtr->QueryInterface(&pDevice);
	if (SUCCEEDED(hr))
		return new CompositorD3D(pDevice);

	return nullptr;
}

CompositorD3D::CompositorD3D(ID3D11Device* pDevice)
{
	m_pDevice = pDevice;
	m_pDevice->GetImmediateContext(m_pContext.GetAddressOf());
}

CompositorD3D::~CompositorD3D()
{
}

TextureBase* CompositorD3D::CreateTexture()
{
	return new TextureD3D(m_pDevice.Get());
}

void CompositorD3D::RenderMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	// TODO: Support mirror textures
}

void CompositorD3D::RenderTextureSwapChain(ovrSession session, ovrEyeType eye, ovrTextureSwapChain swapChain, ovrRecti viewport)
{
	HolographicFramePrediction prediction = session->Frame.CurrentPrediction();
	for (HolographicCameraPose pose : prediction.CameraPoses())
	{
		HolographicCamera cam = pose.HolographicCamera();

		HolographicCameraRenderingParameters renderingParameters = session->Frame.GetRenderingParameters(pose);
		IDirect3DSurface surface = renderingParameters.Direct3D11BackBuffer();

		winrt::com_ptr<ID3D11Texture2D> back_buffer;
		winrt::com_ptr<IDirect3DDxgiInterfaceAccess> dxgiInterfaceAccess = surface.as<IDirect3DDxgiInterfaceAccess>();
		HRESULT hr = dxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(back_buffer.put()));

		D3D11_TEXTURE2D_DESC desc = {};
		back_buffer->GetDesc(&desc);

		CD3D11_BOX box(viewport.Pos.x, viewport.Pos.y, 0, viewport.Size.w, viewport.Size.h, 1);
		TextureD3D* texture = (TextureD3D*)swapChain->Textures[swapChain->SubmitIndex].get();
		m_pContext->CopySubresourceRegion(back_buffer.get(), (UINT)eye, 0, 0, 0, texture->Texture(), 0, &box);
	}
}
