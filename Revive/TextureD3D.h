#pragma once

#include "TextureBase.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <d3d12.h>
#include <d3d11on12.h>

class TextureD3D :
	public TextureBase
{
public:
	TextureD3D(ID3D11Device* pDevice);
	TextureD3D(ID3D11Device* pDevice, ID3D12CommandQueue* pQueue);
	virtual ~TextureD3D();

	virtual void ToVRTexture(vr::Texture_t& texture) override;
	virtual bool Init(ovrTextureType Type, int Width, int Height, int MipLevels, int SampleCount,
		int ArraySize, ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags) override;

	virtual bool LockSharedTexture() override;
	virtual bool UnlockSharedTexture() override;
	virtual bool CreateSharedTextureGL(unsigned int* outName) override;
	virtual void DeleteSharedTextureGL(unsigned int name) override;

	void Acquire() { if (m_pDevice11on12) m_pDevice11on12->AcquireWrappedResources((ID3D11Resource**)m_pTexture.GetAddressOf(), 1); }
	void Release() { if (m_pDevice11on12) m_pDevice11on12->ReleaseWrappedResources((ID3D11Resource**)m_pTexture.GetAddressOf(), 1); }

	IUnknown* Texture() { if (m_pQueue) return m_pResource.Get(); else return m_pTexture.Get(); };
	ID3D11ShaderResourceView* Resource() { return m_pSRV.Get(); };
	ID3D11RenderTargetView* Target() { return m_pRTV.Get(); };

protected:
	static DXGI_FORMAT TextureFormatToDXGIFormat(ovrTextureFormat format, bool typeless = false);
	static UINT BindFlagsToD3DBindFlags(unsigned int flags);
	static UINT MiscFlagsToD3DMiscFlags(unsigned int flags);
	static D3D12_RESOURCE_FLAGS BindFlagsToD3DResourceFlags(unsigned int flags);
	static DXGI_FORMAT ToLinearFormat(DXGI_FORMAT format);
	static DXGI_FORMAT ToColorFormat(DXGI_FORMAT format);

	// DirectX 11
	Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRV;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRTV;

	// DirectX 12
	Microsoft::WRL::ComPtr<ID3D12Device> m_pDevice12;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResolve;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pResolveList;
	vr::D3D12TextureData_t m_data;

	// 11on12 interop
	Microsoft::WRL::ComPtr<ID3D11On12Device> m_pDevice11on12;

	// OpenGL interop
	void* m_hInteropDevice;
	void* m_hInteropTarget;
};
