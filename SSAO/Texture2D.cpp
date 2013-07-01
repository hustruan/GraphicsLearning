#include "DXUT.h"
#include "Texture2D.h"
#include <vector>

namespace {

int WritePfm(const char *fn, int resX, int resY, int channels, const float* data)
{
	if(channels!=1&&channels!=3&&channels!=4)
	{
		return -2;
	}
	FILE* f;
	errno_t err=fopen_s(&f,fn,"wb");
	if(err!=0) return -1;
	const char* indicator;	
	switch(channels)
	{
	case 1:
		indicator="Pf";
		break;
	case 3:
		indicator="PF";
		break;
	case 4:
		indicator="P4";
		break;
	default:
		break;
	}
	fprintf_s(f,"%s\n%d %d\n%f\n",indicator,resX,resY,-1.f);		
	int written=fwrite(data,sizeof(float)*channels,resX*resY,f);
	if(written!=resX*resY)
	{
		fclose(f);
		return -3;
	}
	fclose(f);
	return 0;
}

}

Texture2D::Texture2D( ID3D11Device* d3dDevice, int width, int height, DXGI_FORMAT format, UINT bindFlags /*= D3D11_BIND_SHADER_RESOURCE*/, int mipLevels /*= 1*/ )
	: mRenderTargetView(NULL), mDepthStecilView(NULL), mShaderResourceView(NULL), mDevice(d3dDevice)
{
	InternalConstruct(d3dDevice, width, height, format, bindFlags, mipLevels, 1, 1, 0,
		D3D11_RTV_DIMENSION_TEXTURE2D, D3D11_DSV_DIMENSION_TEXTURE2D, D3D11_SRV_DIMENSION_TEXTURE2D);
}

Texture2D::Texture2D( ID3D11Device* d3dDevice, int width, int height, DXGI_FORMAT format, UINT bindFlags, const DXGI_SAMPLE_DESC& sampleDesc )
{
	mDevice = d3dDevice;

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

void Texture2D::SaveTextureToPfm( ID3D11DeviceContext *pContext, const char* pDestFile  )
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc;
	mTexture->GetDesc(&desc);

	desc.CPUAccessFlags	= D3D11_CPU_ACCESS_READ;
	desc.Usage			= D3D11_USAGE_STAGING;
	desc.BindFlags      =  0;
	ID3D11Texture2D* pTextureStaging;

	hr = mDevice->CreateTexture2D(&desc, NULL, &pTextureStaging);

	pContext->CopyResource( pTextureStaging, mTexture );

	D3D11_MAPPED_SUBRESOURCE texmap;
	hr = pContext->Map(pTextureStaging, 0, D3D11_MAP_READ, 0, &texmap);

	const int width = desc.Width;
	const int height = desc.Height;

	std::vector<float> texData(width * height * 3);

	float* pDest = &texData[0];
	for (int y = 0; y < height; ++y)
	{
		float* pColor = (float*)((unsigned char*)texmap.pData + texmap.RowPitch * (height - 1 - y));
		for (int x = 0; x < width; ++x)
		{
			*pDest++ = *pColor++;
			*pDest++ = *pColor++;
			*pDest++ = *pColor++;

			 pColor++;
		}
	}

	pContext->Unmap(pTextureStaging, 0);

	SAFE_RELEASE(pTextureStaging);

	WritePfm(pDestFile, width, height, 3, &texData[0]);
}
