#include "DXUT.h"
#include "Texture2D.h"

Texture2D::Texture2D( ID3D11Device* d3dDevice, int width, int height, DXGI_FORMAT format, UINT bindFlags /*= D3D11_BIND_SHADER_RESOURCE*/, int mipLevels /*= 1*/ )
	: mRenderTargetView(NULL), mDepthStecilView(NULL), mShaderResourceView(NULL)
{
	InternalConstruct(d3dDevice, width, height, format, bindFlags, mipLevels, 1, 1, 0,
		D3D11_RTV_DIMENSION_TEXTURE2D, D3D11_DSV_DIMENSION_TEXTURE2D, D3D11_SRV_DIMENSION_TEXTURE2D);
}

Texture2D::Texture2D( ID3D11Device* d3dDevice, int width, int height, DXGI_FORMAT format, UINT bindFlags, const DXGI_SAMPLE_DESC& sampleDesc )
{
	InternalConstruct(d3dDevice, width, height, format, bindFlags, 1, 1, sampleDesc.Count, sampleDesc.Quality,
		D3D11_RTV_DIMENSION_TEXTURE2DMS, D3D11_DSV_DIMENSION_TEXTURE2DMS, D3D11_SRV_DIMENSION_TEXTURE2DMS);
}

HRESULT Texture2D::InternalConstruct( ID3D11Device* d3dDevice, int width, int height, DXGI_FORMAT format, UINT bindFlags, int mipLevels, int arraySize, int sampleCount, int sampleQuality, 
								D3D11_RTV_DIMENSION rtvDimension, D3D11_DSV_DIMENSION dsvDimension, D3D11_SRV_DIMENSION srvDimension )
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc = 
	{
		width,
		height,
		mipLevels,
		arraySize,
		format,
		1,     // Sample
		0,
		D3D11_USAGE_DEFAULT, // Usage
		bindFlags,  // BindFlags
		0, // CPUAccessFlags
		(mipLevels != 1) ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0 // MiscFlags
	};

	V_RETURN( d3dDevice->CreateTexture2D(&desc, NULL, &mTexture) );
	mTexture->GetDesc(&desc);

	if (bindFlags & D3D11_BIND_RENDER_TARGET) 
	{
		CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc(rtvDimension, format, 0);
		V_RETURN( d3dDevice->CreateRenderTargetView(mTexture, NULL, &mRenderTargetView) );
	}

	if (bindFlags & D3D11_BIND_DEPTH_STENCIL) 
	{
		DXGI_FORMAT dsvFormat = DXGI_FORMAT_D32_FLOAT;
		switch(format)
		{
		case DXGI_FORMAT_R32_TYPELESS:
			dsvFormat = DXGI_FORMAT_D32_FLOAT;
			break;
		case DXGI_FORMAT_R24G8_TYPELESS:
			dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; 
			break;
		case DXGI_FORMAT_R16_TYPELESS:
			dsvFormat = DXGI_FORMAT_D16_UNORM;
			break;
		default:
			assert(false);
		}

		CD3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc(dsvDimension, dsvFormat, 0);
		V_RETURN( d3dDevice->CreateDepthStencilView(mTexture, &dsvDesc, &mDepthStecilView) );
	}

	if (bindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		DXGI_FORMAT srvFormat = DXGI_FORMAT_R32_FLOAT;

		if (bindFlags & D3D11_BIND_DEPTH_STENCIL) 
		{
			switch(format)
			{
			case DXGI_FORMAT_R32_TYPELESS:
				srvFormat = DXGI_FORMAT_R32_FLOAT;
				break;
			case DXGI_FORMAT_R24G8_TYPELESS:
				srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case DXGI_FORMAT_R16_TYPELESS:
				srvFormat = DXGI_FORMAT_R16_UNORM;
				break;
			default:
				assert(false);
			}
		}
		else
		{
			srvFormat = format;
		}
			
		CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc(srvDimension, srvFormat, 0, desc.MipLevels);
		V_RETURN(  d3dDevice->CreateShaderResourceView(mTexture, &srvDesc, &mShaderResourceView) );
	}

	return S_OK;
}

Texture2D::~Texture2D( void )
{
	SAFE_RELEASE(mRenderTargetView);
	SAFE_RELEASE(mDepthStecilView);
	SAFE_RELEASE(mShaderResourceView);
	SAFE_RELEASE(mTexture);
}
