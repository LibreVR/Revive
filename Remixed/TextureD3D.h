#pragma once

#include "TextureBase.h"

#include <wrl/client.h>
#include <d3d11.h>

class TextureD3D :
	public TextureBase
{
public:
	TextureD3D(ID3D11Device* pDevice);
	virtual ~TextureD3D();

	virtual bool Init(ovrTextureType type, int Width, int Height, int MipLevels, int ArraySize,
		int SampleCount, ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags);

	ID3D11Texture2D* Texture() { return m_pTexture.Get(); };
	ID3D11ShaderResourceView* Resource() { return m_pSRV.Get(); };
	ID3D11RenderTargetView* Target() { return m_pRTV.Get(); };

protected:
	static DXGI_FORMAT ToDXGIFormat(ovrTextureFormat format, unsigned int flags = 0);
	static UINT BindFlagsToD3DBindFlags(unsigned int flags);
	static UINT MiscFlagsToD3DMiscFlags(unsigned int flags);

	// DirectX 11
	Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRV;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRTV;
};
