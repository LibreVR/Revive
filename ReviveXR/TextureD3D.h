#pragma once

#include "TextureBase.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <d3d12.h>

class TextureD3D :
	public TextureBase
{
public:
	TextureD3D(ID3D11Device* pDevice);
	TextureD3D(ID3D12CommandQueue* pQueue);
	virtual ~TextureD3D();

	virtual vr::VRTextureWithPose_t ToVRTexture();
	virtual bool Init(ovrTextureType type, int Width, int Height, int MipLevels, int ArraySize,
		ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags);

	IUnknown* Texture() { if (m_pDevice) return m_pTexture.Get(); else return m_pResource12.Get(); };
	ID3D11ShaderResourceView* Resource() { return m_pSRV.Get(); };
	ID3D11RenderTargetView* Target() { return m_pRTV.Get(); };

protected:
	static DXGI_FORMAT TextureFormatToDXGIFormat(ovrTextureFormat format, bool typeless = false);
	static UINT BindFlagsToD3DBindFlags(unsigned int flags);
	static UINT MiscFlagsToD3DMiscFlags(unsigned int flags);
	static D3D12_RESOURCE_FLAGS BindFlagsToD3DResourceFlags(unsigned int flags);
	static ovrTextureFormat ToLinearFormat(ovrTextureFormat format);

	// DirectX 11
	Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRV;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRTV;

	// DirectX 12
	vr::D3D12TextureData_t m_data;
	Microsoft::WRL::ComPtr<ID3D12Device> m_pDevice12;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource12;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pQueue;
};
