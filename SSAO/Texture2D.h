#pragma once

#include <d3d11.h>

class Texture2D
{
public:
	// Construct a Texture2D
	Texture2D(ID3D11Device* d3dDevice, int width, int height, DXGI_FORMAT format, 
		UINT bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, int mipLevels = 1);

	// Construct a Texture2DMS
	Texture2D(ID3D11Device* d3dDevice,
		int width, int height, DXGI_FORMAT format,
		UINT bindFlags,
		const DXGI_SAMPLE_DESC& sampleDesc);

	~Texture2D(void);


	ID3D11Texture2D* GetTexture() const { return mTexture; }
	ID3D11RenderTargetView* GetRenderTargetView() const { assert(mRenderTargetView); return mRenderTargetView; }
	ID3D11DepthStencilView* GetDepthStencilView() const { assert(mDepthStecilView); return mDepthStecilView; }
	ID3D11ShaderResourceView* GetShaderResourceView() { assert(mShaderResourceView); return mShaderResourceView; }

private:
	// Not implemented
	Texture2D(const Texture2D&);
	Texture2D& operator=(const Texture2D&);

	HRESULT InternalConstruct(ID3D11Device* d3dDevice, int width, int height, DXGI_FORMAT format,
		UINT bindFlags, int mipLevels, int arraySize, int sampleCount, int sampleQuality,
		D3D11_RTV_DIMENSION rtvDimension, D3D11_DSV_DIMENSION dsvDimension, D3D11_SRV_DIMENSION srvDimension);

private:

	ID3D11Texture2D* mTexture;
	
	// shader resource view
	ID3D11ShaderResourceView* mShaderResourceView;;
	
	// render target view
	ID3D11RenderTargetView* mRenderTargetView;;

	// depth stencil view
	ID3D11DepthStencilView* mDepthStecilView;
	
};

