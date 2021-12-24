#pragma once

#include "Swapchain.h"

struct ovrTextureSwapChainD3D12 : public ovrTextureSwapChainData
{
public:
	ovrTextureSwapChainD3D12(struct ID3D12CommandQueue* queue);
	~ovrTextureSwapChainD3D12();

	virtual ovrResult Commit(ovrSession session) override;

	ovrResult InitBarriers();

	static ovrResult Create(ovrSession session, struct ID3D12CommandQueue* queue, const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);

private:
	struct ID3D12CommandQueue* m_Queue;
	struct ID3D12Device* m_Device;
	struct ID3D12CommandAllocator* m_Allocator;
	struct ID3D12GraphicsCommandList** m_AcquireBarriers;
	struct ID3D12GraphicsCommandList** m_ReleaseBarriers;
};
