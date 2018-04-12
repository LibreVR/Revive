#include "CompositorD3D.h"
#include "TextureD3D.h"
#include "Session.h"
#include "FrameList.h"

#include <dxgi1_3.h>
#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include "VertexShader.hlsl.h"
#include "MirrorShader.hlsl.h"
#include "CompositorShader.hlsl.h"

#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
using namespace Windows::Graphics::DirectX::Direct3D11;

#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

#include <winrt/Windows.Graphics.Holographic.h>
using namespace winrt::Windows::Graphics::Holographic;

#include <winrt/Windows.Foundation.h>
// Don't import the namespace, it conflicts

struct Vertex
{
	ovrVector2f Position;
	ovrVector2f TexCoord;
};

CompositorD3D::CompositorD3D()
{
}

CompositorD3D::CompositorD3D(IUnknown* pDevice)
{
	SetDevice(pDevice);
}

CompositorD3D::~CompositorD3D()
{
}

IDirect3DDevice CompositorD3D::GetDevice()
{
	IDirect3DDevice device = nullptr;
	winrt::com_ptr<IDXGIDevice3> pDevice;
	HRESULT hr = m_pDevice->QueryInterface(pDevice.put());
	if (SUCCEEDED(hr))
	{
		winrt::com_ptr<IInspectable> inspectableDevice;
		hr = CreateDirect3D11DeviceFromDXGIDevice(pDevice.get(), inspectableDevice.put());
		device = inspectableDevice.as<IDirect3DDevice>();
	}
	return device;
}

bool CompositorD3D::InitDevice()
{
	HolographicDisplay display = HolographicDisplay::GetDefault();
	HolographicAdapterId id = display.AdapterId();

	IDXGIFactory1* pFactory = nullptr;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);
	if (FAILED(hr))
		return false;

	Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
	for (int i = 0; pFactory->EnumAdapters1(i, pAdapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; i++)
	{
		DXGI_ADAPTER_DESC1 desc;
		if (SUCCEEDED(pAdapter->GetDesc1(&desc)))
		{
			if (id.HighPart == desc.AdapterLuid.HighPart &&
				id.LowPart == desc.AdapterLuid.LowPart)
				break;
		}
		pAdapter = nullptr;
	}

	ID3D11Device* pDevice = nullptr;
	hr = D3D11CreateDevice(pAdapter.Get(), pAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &pDevice, nullptr, nullptr);
	if (FAILED(hr))
		return false;
	SetDevice(pDevice);
	return true;
}

bool CompositorD3D::SetDevice(IUnknown* pDevice)
{
	if (m_pDevice.Get() == pDevice)
		return false;

	pDevice->QueryInterface(m_pDevice.ReleaseAndGetAddressOf());
	m_pDevice->GetImmediateContext(m_pContext.ReleaseAndGetAddressOf());

	// Create the shaders.
	m_pDevice->CreateVertexShader(g_VertexShader, sizeof(g_VertexShader), NULL, m_VertexShader.ReleaseAndGetAddressOf());
	m_pDevice->CreatePixelShader(g_MirrorShader, sizeof(g_MirrorShader), NULL, m_MirrorShader.ReleaseAndGetAddressOf());
	m_pDevice->CreatePixelShader(g_CompositorShader, sizeof(g_CompositorShader), NULL, m_CompositorShader.ReleaseAndGetAddressOf());

	// Create the vertex buffer.
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(Vertex) * 4;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	m_pDevice->CreateBuffer(&bufferDesc, nullptr, m_VertexBuffer.ReleaseAndGetAddressOf());

	// Create the input layout.
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
		D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	m_pDevice->CreateInputLayout(layout, 2, g_VertexShader, sizeof(g_VertexShader), m_InputLayout.ReleaseAndGetAddressOf());

	// Create state objects.
	D3D11_BLEND_DESC bm = { 0 };
	bm.RenderTarget[0].BlendEnable = true;
	bm.RenderTarget[0].BlendOp = bm.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bm.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	bm.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bm.RenderTarget[0].SrcBlendAlpha = bm.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bm.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	m_pDevice->CreateBlendState(&bm, m_BlendState.ReleaseAndGetAddressOf());

	return true;
}

TextureBase* CompositorD3D::CreateTexture()
{
	return new TextureD3D(m_pDevice.Get());
}

void CompositorD3D::RenderMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	// TODO: Support mirror textures
}

void CompositorD3D::RenderTextureSwapChain(IDirect3DSurface surface, ovrTextureSwapChain swapChain, ovrRecti viewport, ovrEyeType eye)
{
	Direct3DSurfaceDescription surface_desc = surface.Description();

	UINT subresource = 0;
	winrt::com_ptr<ID3D11Texture2D> back_buffer;
	winrt::com_ptr<IDXGISurface2> dxgi_surface;
	winrt::com_ptr<IDirect3DDxgiInterfaceAccess> dxgiInterfaceAccess = surface.as<IDirect3DDxgiInterfaceAccess>();
	HRESULT hr;
	hr = dxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(dxgi_surface.put()));
	if (dxgi_surface)
		hr = dxgi_surface->GetResource(IID_PPV_ARGS(back_buffer.put()), &subresource);
	else
		hr = dxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(back_buffer.put()));

	TextureD3D* texture = (TextureD3D*)swapChain->Textures[swapChain->SubmitIndex].get();

	// Get the current state objects
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state;
	float blend_factor[4];
	uint32_t sample_mask;
	m_pContext->OMGetBlendState(blend_state.GetAddressOf(), blend_factor, &sample_mask);

	D3D11_PRIMITIVE_TOPOLOGY topology;
	m_pContext->IAGetPrimitiveTopology(&topology);

	ID3D11RasterizerState* ras_state;
	m_pContext->RSGetState(&ras_state);

	// Set the compositor shaders
	m_pContext->VSSetShader(m_VertexShader.Get(), NULL, 0);
	m_pContext->PSSetShader(m_CompositorShader.Get(), NULL, 0);
	ID3D11ShaderResourceView* resource = texture->Resource();
	m_pContext->PSSetShaderResources(0, 1, &resource);

	// Update the vertex buffer
	// TODO: Account for the Field-of-View
	float w = (float)swapChain->Desc.Width;
	float h = (float)swapChain->Desc.Height;
	ovrVector2f min = { viewport.Pos.x / w, viewport.Pos.y / h };
	ovrVector2f max = { (viewport.Pos.x + viewport.Size.w) / w, (viewport.Pos.y + viewport.Size.h) / h };
	Vertex vertices[4] = {
		{ { -1.0f,  1.0f },{ min.x, min.y } },
		{ {  1.0f,  1.0f },{ max.x, min.y } },
		{ { -1.0f, -1.0f },{ min.x, max.y } },
		{ {  1.0f, -1.0f },{ max.x, max.y } }
	};
	D3D11_MAPPED_SUBRESOURCE map = { 0 };
	m_pContext->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	memcpy(map.pData, vertices, sizeof(Vertex) * 4);
	m_pContext->Unmap(m_VertexBuffer.Get(), 0);

	// Prepare the render target
	winrt::com_ptr<ID3D11RenderTargetView> rtv;
	D3D11_RENDER_TARGET_VIEW_DESC target_desc = {};
	target_desc.Format = TextureBase::IsSRGBFormat(swapChain->Desc.Format) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
	target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	target_desc.Texture2DArray.MipSlice = 0;
	target_desc.Texture2DArray.FirstArraySlice = (UINT)eye;
	target_desc.Texture2DArray.ArraySize = 1;
	m_pDevice->CreateRenderTargetView(back_buffer.get(), &target_desc, rtv.put());
	FLOAT clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_pContext->ClearRenderTargetView(rtv.get(), clear);

	ID3D11RenderTargetView* targets[] = { rtv.get() };
	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)surface_desc.Width, (float)surface_desc.Height, D3D11_MIN_DEPTH, D3D11_MIN_DEPTH };
	m_pContext->RSSetViewports(1, &vp);
	m_pContext->OMSetRenderTargets(1, targets, nullptr);
	m_pContext->OMSetBlendState(m_BlendState.Get(), nullptr, -1);
	m_pContext->RSSetState(nullptr);

	// Set and draw the vertices
	uint32_t stride = sizeof(Vertex);
	uint32_t offset = 0;
	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pContext->IASetInputLayout(m_InputLayout.Get());
	m_pContext->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	m_pContext->Draw(4, 0);

	// Restore the state objects
	m_pContext->RSSetState(ras_state);
	m_pContext->OMSetBlendState(blend_state.Get(), blend_factor, sample_mask);
	m_pContext->IASetPrimitiveTopology(topology);
}
