#include "SwapchainD3D12.h"
#include "Common.h"
#include "Session.h"

#define XR_USE_GRAPHICS_API_D3D12
#include <d3d12.h>
#include <openxr/openxr_platform.h>

ovrTextureSwapChainD3D12::ovrTextureSwapChainD3D12(struct ID3D12CommandQueue* queue)
	: m_Queue(queue)
	, m_Device()
	, m_Allocator()
	, m_AcquireBarriers()
	, m_ReleaseBarriers()
{
	if (m_Queue)
		m_Queue->AddRef();

	if (m_Queue && SUCCEEDED(m_Queue->GetDevice(IID_PPV_ARGS(&m_Device))))
		m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_Allocator));
}

ovrTextureSwapChainD3D12::~ovrTextureSwapChainD3D12()
{
	for (uint32_t i = 0; i < Length; i++)
	{
		m_AcquireBarriers[i]->Release();
		m_ReleaseBarriers[i]->Release();
	}
	delete[] m_AcquireBarriers;
	delete[] m_ReleaseBarriers;

	if (m_Allocator)
		m_Allocator->Release();

	if (m_Device)
		m_Device->Release();

	if (m_Queue)
		m_Queue->Release();
}

ovrResult ovrTextureSwapChainD3D12::Commit(ovrSession session)
{
	m_Queue->ExecuteCommandLists(1, (ID3D12CommandList**)&m_ReleaseBarriers[CurrentIndex]);
	ovrResult result = ovrTextureSwapChainData::Commit(session);
	m_Queue->ExecuteCommandLists(1, (ID3D12CommandList**)&m_AcquireBarriers[CurrentIndex]);
	return result;
}

ovrResult ovrTextureSwapChainD3D12::InitBarriers()
{
	m_AcquireBarriers = new ID3D12GraphicsCommandList*[Length];
	m_ReleaseBarriers = new ID3D12GraphicsCommandList*[Length];

	for (uint32_t i = 0; i < Length; i++)
	{
		XrSwapchainImageD3D12KHR image = ((XrSwapchainImageD3D12KHR*)Images)[i];

		D3D12_RESOURCE_STATES target = IsDepthFormat(Desc.Format) ?
			D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;

		CHK_HR(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_Allocator,
			nullptr, IID_PPV_ARGS(&m_AcquireBarriers[i])));

		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = image.texture;
		barrier.Transition.StateBefore = target;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_AcquireBarriers[i]->ResourceBarrier(1, &barrier);
		m_AcquireBarriers[i]->Close();

		CHK_HR(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_Allocator,
			nullptr, IID_PPV_ARGS(&m_ReleaseBarriers[i])));

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = target;
		m_ReleaseBarriers[i]->ResourceBarrier(1, &barrier);
		m_ReleaseBarriers[i]->Close();
	}

	m_Queue->ExecuteCommandLists(1, (ID3D12CommandList**)&m_AcquireBarriers[CurrentIndex]);

	return ovrSuccess;
}

ovrResult ovrTextureSwapChainD3D12::Create(ovrSession session, ID3D12CommandQueue* queue, const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	// Do some format compatibility conversions before creating the swapchain
	DXGI_FORMAT format = NegotiateFormat(session, TextureFormatToDXGIFormat(desc->Format));
	assert(session->SupportsFormat(format));

	ovrTextureSwapChainD3D12* chain = new ovrTextureSwapChainD3D12(queue);
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

	CHK_HR(chain->InitBarriers());

	*out_TextureSwapChain = chain;
	return ovrSuccess;
}
