#include "DXUT.h"
#include "SSAO.h"
#include "Texture2D.h"
#include "Shader.h"
#include "DXUTcamera.h"
#include "SDKMesh.h"
#include "LightAnimation.h"
#include "Scene.h"
#include "Utility.h"

#include <random>
#include <cstdint>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// NOTE: Must match layout of shader constant buffers

__declspec(align(16))
struct PerObjectConstants
{
	D3DXMATRIX WorldView;
	D3DXMATRIX WorldViewProj;
};

__declspec(align(16))
struct SSAOParams
{
	float DefaultAO;
	float Radius;
	float NormRange;
	float DeltaSacle;
};


// CBuffer use a Float4Align
#pragma warning(push)
#pragma warning( disable : 4324 )
struct LightCBuffer
{
	Float4Align D3DXVECTOR3 LightColor;
	Float4Align D3DXVECTOR3 LightPosition;        // View space light position
	Float4Align D3DXVECTOR3 LightDirection;       // View space normalized direction 
	Float4Align D3DXVECTOR3 SpotFalloff;
	Float4Align D3DXVECTOR2 LightAttenuation;         // begin and end
};
#pragma warning( pop ) 

SSAO::SSAO( ID3D11Device* d3dDevice )
	: mDepthBufferReadOnlyDSV(0), mHBAORandomSRV(0), mHBAORandomTexture(0), mNoiseSRV(0), mBestFitNormalSRV(0),
	  mLightPrePass(false), mCullTechnique(Cull_Forward_None), mLightingMethod(Lighting_Forward), mShowAO(false), mUseSSAO(true)
{
	mAOOffsetScale = 0.001;

	CreateShaderEffect(d3dDevice);

	CreateRenderStates(d3dDevice);

	CreateConstantBuffers(d3dDevice);	

	// Load light volume mesh 
	{
		HRESULT hr;

		mPointLightProxy = new CDXUTSDKMesh;
		mSpotLightProxy = new CDXUTSDKMesh;
		
		V(mPointLightProxy->Create( d3dDevice, L".\\Media\\PointLightProxy.sdkmesh"));
		V(mSpotLightProxy->Create( d3dDevice, L".\\Media\\SpotLightProxy.sdkmesh"));
	}

	// Load noise texture
	SAFE_RELEASE(mNoiseSRV);
	D3DX11CreateShaderResourceViewFromFile( d3dDevice, L".\\Media\\Textures\\vector_noise.dds", NULL, NULL, &mNoiseSRV, NULL );
	DXUT_SetDebugName(mNoiseSRV, "mNoiseSRV");

	SAFE_RELEASE(mBestFitNormalSRV);
	D3DX11CreateShaderResourceViewFromFile( d3dDevice, L".\\Media\\Textures\\BestFitNormal.dds", NULL, NULL, &mBestFitNormalSRV, NULL );
	DXUT_SetDebugName(mNoiseSRV, "mBestFitNormalSRV");

	CreateHBAORandomTexture(d3dDevice);
}

SSAO::~SSAO(void)
{
	SAFE_RELEASE(mMeshVertexLayout);
	SAFE_RELEASE(mLightProxyVertexLayout);
	
	SAFE_RELEASE(mDiffuseSampler);
	SAFE_RELEASE(mPointClampSampler);
	SAFE_RELEASE(mPointWarpSampler);

	SAFE_RELEASE(mRasterizerFrontState);
	SAFE_RELEASE(mRasterizerState);

	SAFE_RELEASE(mDepthState);
	SAFE_RELEASE(mDepthDisableState);
	SAFE_RELEASE(mDepthLEQualState);
	SAFE_RELEASE(mDepthGreaterState);
	SAFE_RELEASE(mVolumeStencilState);

	SAFE_RELEASE(mPerFrameConstants);
	SAFE_RELEASE(mAOParamsConstants);
	SAFE_RELEASE(mPerObjectConstants);
	SAFE_RELEASE(mLightConstants);
	SAFE_RELEASE(mHBAOParamsConstant);
	SAFE_RELEASE(mBlurParamsConstants);

	SAFE_RELEASE(mNoiseSRV);
	SAFE_RELEASE(mBestFitNormalSRV);
	SAFE_RELEASE(mDepthBufferReadOnlyDSV);
	SAFE_RELEASE(mHBAORandomSRV);
	SAFE_RELEASE(mHBAORandomTexture);
	
	SAFE_RELEASE(mGeometryBlendState);
	SAFE_RELEASE(mLightingBlendState);

	
	delete mPointLightProxy;	
	delete mSpotLightProxy;	
}

void SSAO::CreateShaderEffect( ID3D11Device* d3dDevice )
{
	mFullQuadSprite = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\FullQuadSprite.hlsl", "FullQuadSpritePS", nullptr);
	mFullQuadSpriteAO = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\FullQuadSprite.hlsl", "FullQuadSpriteAOPS", nullptr);

	mGBufferVS = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\GBuffer.hlsl", "GBufferVS", nullptr);
	mGBufferPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\GBuffer.hlsl", "GBufferPS", nullptr);

	mFullScreenTriangleVS = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\FullScreenTriangle.hlsl", "FullScreenTriangleVS", nullptr);
	mSSAOCrytekPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\SSAO.hlsl", "SSAOPS", nullptr);
	mHBAOPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\HBAO.hlsl", "HBAO", nullptr);
	mAlchemyAOPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\AlchemyAO.hlsl", "AlchemyAO", nullptr);

	mBlurXPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\CrossBilateralFilter.hlsl", "BlurX", nullptr);
	mBlurYPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\CrossBilateralFilter.hlsl", "BlurY", nullptr);

	mEdgeAAPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\EdgeDetect.hlsl", "DL_GetEdgeWeight", nullptr);

	mForwardDirectionalVS = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\ForwardRendering.hlsl", "ForwardVS", nullptr);
	mForwardDirectionalPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\ForwardRendering.hlsl", "ForwardPS", nullptr);

	mScreenQuadVS = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\GPUScreenQuad.hlsl", "GPUQuadVS", nullptr);
	mScreenQuadGS = ShaderFactory::CreateShader<GeometryShader>(d3dDevice, L".\\Media\\Shaders\\GPUScreenQuad.hlsl", "GPUQuadGS", nullptr);

	//D3D11_SO_DECLARATION_ENTRY streamOutputEntry[] =
	//{
	//	{0, "TEXCOORD",    0, 0, 3, 0},
	//	{0, "TEXCOORD",    1, 0, 3, 0},
	//	{0, "SV_Position", 0, 0, 4, 0},
	//};
	//
	//UINT bufferStrides[] = { sizeof(D3DXVECTOR3) + sizeof(D3DXVECTOR3) + sizeof(D3DXVECTOR4) };
	//mScreenQuadGS = ShaderFactory::CreateGeometryShaderWithStreamOutput(d3dDevice, streamOutputEntry, 3, bufferStrides, 1, 0, 
	//	L".\\Media\\Shaders\\GPUScreenQuad.hlsl", "GPUQuadGS", nullptr);

	{
		D3D10_SHADER_MACRO defines[] = {
			{"PointLight", ""},
			{0, 0}
		};
		mDeferredShadingVS[LT_PointLight] = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\DeferredShadingClassicVS.hlsl", "DeferredRenderingVS", defines);
		mDeferredShadingPS[LT_PointLight] = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredShadingClassicPS.hlsl", "DeferredRenderingPS", defines);
		mDeferredLightingPS[LT_PointLight] = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredLightingPassPS.hlsl", "DeferredLightingPS", defines);
	}

	{
		D3D10_SHADER_MACRO defines[] = {
			{"SpotLight", ""},
			{0, 0}
		};

		mDeferredShadingVS[LT_SpotLight] = mDeferredShadingVS[LT_PointLight];
		//mDeferredPointPS = std::make_shared<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredRendering.hlsl", "DeferredRenderingPointPS", nullptr);
		mDeferredLightingPS[LT_SpotLight] = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredLightingPassPS.hlsl", "DeferredLightingPS", defines);
	}

	{
		D3D10_SHADER_MACRO defines[] = {
			{"DirectionalLight", ""},
			{0, 0}
		};
		mDeferredShadingVS[LT_DirectionalLigt] = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\DeferredShadingClassicVS.hlsl", "DeferredRenderingVS", defines);
		mDeferredShadingPS[LT_DirectionalLigt] = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredShadingClassicPS.hlsl", "DeferredRenderingPS", defines);
		mDeferredLightingPS[LT_DirectionalLigt] = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredLightingPassPS.hlsl", "DeferredLightingPS", defines);
	}

	// Deferred light shading pass
	mDeferredLightingShadingPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredShadingPassPS.hlsl", "DeferredShadingPS", nullptr);

	mDebugVS = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\DebugVolumeVS.hlsl", "DebugPointLightVS", nullptr);
	mDebugPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DebugVolumePS.hlsl", "DebugPointLightPS", nullptr);

	// Create mesh vertex layout
	{	
		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		mMeshVertexLayout = ShaderFactory::CreateVertexLayout(d3dDevice, layout, ARRAYSIZE(layout),  L".\\Media\\Shaders\\GBuffer.hlsl", "GBufferVS", NULL);
		DXUT_SetDebugName(mMeshVertexLayout, "mMeshVertexLayout");
	}

	{
		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		D3D10_SHADER_MACRO defines[] = {
			{"PointLight", ""},
			{"SpotLight", ""},
			{0, 0}
		};
		mLightProxyVertexLayout = ShaderFactory::CreateVertexLayout(d3dDevice, layout, ARRAYSIZE(layout),  L".\\Media\\Shaders\\DeferredShadingClassicVS.hlsl", "DeferredRenderingVS", defines);
		DXUT_SetDebugName(mLightProxyVertexLayout, "mLightProxyVertexLayout");
	}
}

void SSAO::CreateConstantBuffers( ID3D11Device* d3dDevice )
{
	{
		CD3D11_BUFFER_DESC desc(sizeof(PerFrameConstants), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		d3dDevice->CreateBuffer(&desc, nullptr, &mPerFrameConstants);
		DXUT_SetDebugName(mPerFrameConstants, "mPerFrameConstants");
	}

	{
		CD3D11_BUFFER_DESC desc(sizeof(PerObjectConstants), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		d3dDevice->CreateBuffer(&desc, nullptr, &mPerObjectConstants);
		DXUT_SetDebugName(mPerObjectConstants, "mPerObjectConstants");
	}

	{
		CD3D11_BUFFER_DESC desc(sizeof(SSAOParams), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		d3dDevice->CreateBuffer(&desc, nullptr, &mAOParamsConstants);
		DXUT_SetDebugName(mAOParamsConstants, "mAOParamsConstants");
	}

	{
		mHBAOParams.Radius = 1.0f;
		mHBAOParams.TanAngleBias = tanf(D3DXToRadian(10));

		CD3D11_BUFFER_DESC desc(sizeof(HBAOParams), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		d3dDevice->CreateBuffer(&desc, nullptr, &mHBAOParamsConstant);
		DXUT_SetDebugName(mHBAOParamsConstant, "mHBAOParams");
	}

	{
		mBlurParams.BlurRadius = 7.0f;
		
		float sharpness = 18.0f;
		mBlurParams.BlurSharpness = sharpness * sharpness;

		CD3D11_BUFFER_DESC desc(sizeof(BlurParams), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		d3dDevice->CreateBuffer(&desc, nullptr, &mBlurParamsConstants);
		DXUT_SetDebugName(mBlurParamsConstants, "mBlurParamsConstants");
	}

	//{
	//	CD3D11_BUFFER_DESC desc(sizeof(float)*10*10, D3D11_BIND_STREAM_OUTPUT, D3D11_USAGE_DEFAULT, 0);
	//	d3dDevice->CreateBuffer(&desc, nullptr, &mStreamOutputGPU);
	//	DXUT_SetDebugName(mStreamOutputGPU, "mStreamOutputGPU");

	//	desc.BindFlags		= 0;
	//	desc.CPUAccessFlags	= D3D11_CPU_ACCESS_READ;
	//	desc.Usage			= D3D11_USAGE_STAGING;
	//	desc.StructureByteStride = 0;
	//	desc.MiscFlags = 0;

	//	d3dDevice->CreateBuffer(&desc, nullptr, &mStreamOutputCPU);
	//	DXUT_SetDebugName(mStreamOutputCPU, "mStreamOutputCPU");
	//}

	{
		CD3D11_BUFFER_DESC desc(sizeof(LightCBuffer), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		d3dDevice->CreateBuffer(&desc, nullptr, &mLightConstants);
		DXUT_SetDebugName(mLightConstants, "mPointLightConstants");
	}



}

void SSAO::CreateRenderStates( ID3D11Device* d3dDevice )
{
	// Create standard rasterizer state, cull back face
	{
		CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
		d3dDevice->CreateRasterizerState(&desc, &mRasterizerState);
		DXUT_SetDebugName(mRasterizerState, "mRasterizerState");
	}

	// Cull front face, draw back face
	{
		CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
		desc.CullMode = D3D11_CULL_FRONT;
		d3dDevice->CreateRasterizerState(&desc, &mRasterizerFrontState);
		DXUT_SetDebugName(mRasterizerFrontState, "mRasterizerFrontState");
	}

	// Create standard depth state
	{
		CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
		d3dDevice->CreateDepthStencilState(&desc, &mDepthState);
		DXUT_SetDebugName(mDepthState, "mDepthState");
	}

	// Create depth disable
	{
		CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
		desc.DepthEnable = FALSE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		d3dDevice->CreateDepthStencilState(&desc, &mDepthDisableState);
		DXUT_SetDebugName(mDepthDisableState, "mDepthDisableState");
	}

	// Greater
	{
		CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
		desc.DepthEnable = TRUE;
		desc.DepthFunc = D3D11_COMPARISON_GREATER;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		d3dDevice->CreateDepthStencilState(&desc, &mDepthGreaterState);
		DXUT_SetDebugName(mDepthGreaterState, "mDepthGreaterState");
	}

	// Less Equal
	{
		CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
		desc.DepthEnable = TRUE;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		d3dDevice->CreateDepthStencilState(&desc, &mDepthLEQualState);
		DXUT_SetDebugName(mDepthLEQualState, "mDepthLEQualState");
	}

	{
		CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		desc.StencilEnable = TRUE;
		desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		d3dDevice->CreateDepthStencilState(&desc, &mVolumeStencilState);
		DXUT_SetDebugName(mVolumeStencilState, "mVolumeStencilState");
	}

	// Create geometry phase blend state
	{
		CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
		d3dDevice->CreateBlendState(&desc, &mGeometryBlendState);
		DXUT_SetDebugName(mGeometryBlendState, "mGeometryBlendState");
	}

	{
		CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		d3dDevice->CreateBlendState(&desc, &mLightingBlendState);
		DXUT_SetDebugName(mLightingBlendState, "mLightingBlendState");
	}

	// Create sampler state
	{
		CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
		desc.Filter = D3D11_FILTER_ANISOTROPIC;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MaxAnisotropy = 16;
		d3dDevice->CreateSamplerState(&desc, &mDiffuseSampler);
		DXUT_SetDebugName(mDiffuseSampler, "mDiffuseSampler");
	}

	// Create sampler for random vector
	{
		CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		d3dDevice->CreateSamplerState(&desc, &mPointWarpSampler);
		DXUT_SetDebugName(mPointWarpSampler, "mPointWarpSampler");
	}

	// Create sampler for GBuffer
	{
		CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
		desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		d3dDevice->CreateSamplerState(&desc, &mPointClampSampler);	
		DXUT_SetDebugName(mPointClampSampler, "mPointClampSampler");
	}
}

void SSAO::OnD3D11ResizedSwapChain( ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferDesc )
{
	mGBufferWidth = backBufferDesc->Width;
	mGBufferHeight = backBufferDesc->Height;

	SAFE_RELEASE(mDepthBufferReadOnlyDSV);

	mAOBuffer = std::make_shared<Texture2D>(d3dDevice, backBufferDesc->Width, backBufferDesc->Height,
		DXGI_FORMAT_R32_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

	mBlurBuffer = std::make_shared<Texture2D>(d3dDevice, backBufferDesc->Width, backBufferDesc->Height,
		DXGI_FORMAT_R32_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

	// lit buffers
	mLitBuffer = std::make_shared<Texture2D>(d3dDevice, backBufferDesc->Width, backBufferDesc->Height,
		DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

	// deferred lighting accumulation buffer
	mLightAccumulateBuffer = std::make_shared<Texture2D>(d3dDevice, backBufferDesc->Width, backBufferDesc->Height,
		DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

	mDepthBuffer = std::make_shared<Texture2D>(d3dDevice, backBufferDesc->Width, backBufferDesc->Height,
		DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL);

	// read-only depth stencil view
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		mDepthBuffer->GetDepthStencilView()->GetDesc(&desc);
		desc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
		d3dDevice->CreateDepthStencilView(mDepthBuffer->GetTexture(), &desc, &mDepthBufferReadOnlyDSV);
	}

	// Create GBuffer
	mGBuffer.clear();

	// normals and depth
	mGBuffer.push_back(std::make_shared<Texture2D>(d3dDevice, backBufferDesc->Width, backBufferDesc->Height,
		DXGI_FORMAT_R8G8B8A8_UNORM , D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET));

	/*mGBuffer.push_back(std::make_shared<Texture2D>(d3dDevice, backBufferDesc->Width, backBufferDesc->Height,
		DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET));*/

	// albedo
	mGBuffer.push_back(std::make_shared<Texture2D>(d3dDevice, backBufferDesc->Width, backBufferDesc->Height,
		DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET));

	mGBufferRTV.resize(mGBuffer.size());
	mGBufferSRV.resize(mGBuffer.size());
	for (size_t i = 0; i < mGBuffer.size(); ++i)
	{
		mGBufferRTV[i] = mGBuffer[i]->GetRenderTargetView();
		mGBufferSRV[i] = mGBuffer[i]->GetShaderResourceView();
	}
}

void SSAO::RenderForward( ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth, const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	d3dDeviceContext->ClearRenderTargetView(backBuffer, zeros);

	d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

	d3dDeviceContext->VSSetShader(mForwardDirectionalVS->GetShader(), 0, 0);
	d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerObjectConstants);
	
	d3dDeviceContext->PSSetShader(mForwardDirectionalPS->GetShader(), 0, 0);
	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mLightConstants);
	d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
	
	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	d3dDeviceContext->OMSetDepthStencilState(mDepthState, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
	d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, backDepth);

	const D3DXMATRIX& cameraProj = *viewerCamera.GetProjMatrix();
	const D3DXMATRIX& cameraView = *viewerCamera.GetViewMatrix();

	// Find first directional light index
	size_t idx = 0;
	while(idx < lights.mLights.size() && lights.mLights[idx].LightType != LT_DirectionalLigt) idx++;

	while(idx < lights.mLights.size() && lights.mLights[idx].LightType == LT_DirectionalLigt)
	{
		const D3DXVECTOR3& lightDiection = lights.mLights[idx].LightDirection;

		// Fill DirectionalLight constants
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mLightConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		LightCBuffer* lightCBffer = static_cast<LightCBuffer*>(mappedResource.pData);	

		D3DXVec3TransformNormal(&lightCBffer->LightDirection, &lightDiection, &cameraView);
		lightCBffer->LightColor = lights.mLights[idx].LightColor;
		d3dDeviceContext->Unmap(mLightConstants, 0);

		for (size_t i = 0; i < scene.mSceneMeshesOpaque.size(); ++i)
		{
			// Fill per object constants
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mPerObjectConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			PerObjectConstants* constants = static_cast<PerObjectConstants *>(mappedResource.pData);

			constants->WorldView = scene.mSceneMeshesOpaque[i].World * cameraView;
			constants->WorldViewProj = constants->WorldView * cameraProj;

			d3dDeviceContext->Unmap(mPerObjectConstants, 0);

			scene.mSceneMeshesOpaque[i].Mesh->Render(d3dDeviceContext, 0);
		}

		idx++;
	}	
}

void SSAO::RenderDeferred( ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth, const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	// Generate GBuffer
	RenderGBuffer(d3dDeviceContext, scene, viewerCamera, viewport);

	if (mUseSSAO || mShowAO)
	{
		switch(mAOTechnique)
		{
		case AO_Cryteck:
			//RenderSSAO(d3dDeviceContext, viewerCamera, viewport);
			//break;
		case AO_HBAO:
			RenderHBAO(d3dDeviceContext, viewerCamera, viewport);
			break;
		case AO_Alchemy:
			RenderAlchemyAO(d3dDeviceContext, viewerCamera, viewport);
			break;
		}	
	}
	
	if (!mShowAO)
	{
		// Deferred shading
		ComputeShading(d3dDeviceContext, lights, viewerCamera, viewport);	
	}
	
	// Post-Process
	PostProcess(d3dDeviceContext, backBuffer, backDepth, viewport);
}

void SSAO::Render( ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
	const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	// Fill PerFrameContant
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		PerFrameConstants* constants = static_cast<PerFrameConstants *>(mappedResource.pData);

		constants->Proj = *viewerCamera.GetProjMatrix();
		D3DXMatrixInverse(&constants->InvProj, NULL, viewerCamera.GetProjMatrix());
		constants->NearFar = D3DXVECTOR2(viewerCamera.GetNearClip(), viewerCamera.GetFarClip());

		constants->UseSSAO = mUseSSAO;
		constants->ShowAO = false;

		d3dDeviceContext->Unmap(mPerFrameConstants, 0);
	}

	if (mLightingMethod == Lighting_Forward)
		RenderForward(d3dDeviceContext, backBuffer, backDepth, scene, lights, viewerCamera, viewport);
	else
		RenderDeferred(d3dDeviceContext, backBuffer, backDepth, scene, lights, viewerCamera, viewport);
}

void SSAO::RenderGBuffer( ID3D11DeviceContext* d3dDeviceContext, const Scene& scene, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	// Clear GBuffer
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	for (size_t i = 0; i < mGBufferRTV.size(); ++i)
		d3dDeviceContext->ClearRenderTargetView(mGBufferRTV[i], zeros);

	// Clear Depth 
	d3dDeviceContext->ClearDepthStencilView(mDepthBuffer->GetDepthStencilView(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

	d3dDeviceContext->VSSetShader(mGBufferVS->GetShader(), 0, 0);
	d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerObjectConstants);

	d3dDeviceContext->PSSetShader(mGBufferPS->GetShader(), 0, 0);
	d3dDeviceContext->PSSetShaderResources(0, 1, &mBestFitNormalSRV);
	d3dDeviceContext->PSSetSamplers(0, 1, &mPointClampSampler); // Point Sampler
	d3dDeviceContext->PSSetSamplers(1, 1, &mDiffuseSampler);
	
	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	d3dDeviceContext->OMSetDepthStencilState(mDepthState, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
	d3dDeviceContext->OMSetRenderTargets(mGBufferRTV.size(), &mGBufferRTV[0], mDepthBuffer->GetDepthStencilView());

	const D3DXMATRIX& cameraProj = *viewerCamera.GetProjMatrix();
	const D3DXMATRIX& cameraView = *viewerCamera.GetViewMatrix();
	
	for (size_t i = 0; i < scene.mSceneMeshesOpaque.size(); ++i)
	{
		// Fill per object constants
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mPerObjectConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		PerObjectConstants* constants = static_cast<PerObjectConstants *>(mappedResource.pData);

		constants->WorldView = scene.mSceneMeshesOpaque[i].World * cameraView;
		constants->WorldViewProj = constants->WorldView * cameraProj;

		d3dDeviceContext->Unmap(mPerObjectConstants, 0);

		scene.mSceneMeshesOpaque[i].Mesh->Render(d3dDeviceContext, 1);
	}
	
	// Cleanup (aka make the runtime happy)
	d3dDeviceContext->VSSetShader(0, 0, 0);
	d3dDeviceContext->PSSetShader(0, 0, 0);
	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
	ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
	d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
	ID3D11Buffer* nullBuffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetConstantBuffers(0, 8, nullBuffer);
	d3dDeviceContext->PSSetConstantBuffers(0, 8, nullBuffer);
}

void SSAO::RenderSSAO( ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
	d3dDeviceContext->ClearRenderTargetView(mLitBuffer->GetRenderTargetView(), zeros);
	
	// Fill AO constants
	{
		auto s = sizeof SSAOParams;

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mAOParamsConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		SSAOParams* constants = static_cast<SSAOParams *>(mappedResource.pData);

		constants->DefaultAO = 0.5f;
		constants->Radius = 0.5f;
		constants->NormRange = 3.0f;
		constants->DeltaSacle = 200.0f;

		d3dDeviceContext->Unmap(mAOParamsConstants, 0);
	}

	// Render full sreen quad
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetConstantBuffers(1, 1, &mAOParamsConstants);

	ID3D11ShaderResourceView* srv[3] = { mGBufferSRV[0], mNoiseSRV, mGBufferSRV[2] };
	d3dDeviceContext->PSSetShaderResources(0, 3, srv);
	ID3D11SamplerState* samplers[2] = { mPointClampSampler, mPointWarpSampler };
	d3dDeviceContext->PSSetSamplers(0, 2, samplers);
	d3dDeviceContext->PSSetShader(mSSAOCrytekPS->GetShader(), 0, 0);

	ID3D11RenderTargetView * renderTargets[1] = { mLitBuffer->GetRenderTargetView() };
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
	d3dDeviceContext->OMSetDepthStencilState(mDepthDisableState, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

	d3dDeviceContext->Draw(3, 0);

	// Cleanup (aka make the runtime happy)
	d3dDeviceContext->VSSetShader(0, 0, 0);
	d3dDeviceContext->PSSetShader(0, 0, 0);
	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
	ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
	d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
	ID3D11Buffer* nullBuffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetConstantBuffers(0, 8, nullBuffer);
	d3dDeviceContext->PSSetConstantBuffers(0, 8, nullBuffer);
}

void SSAO::RenderHBAO( ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
	d3dDeviceContext->ClearRenderTargetView(mAOBuffer->GetRenderTargetView(), zeros);

	const D3DXMATRIX& cameraProj = *viewerCamera.GetProjMatrix();

	// Fill AO constants
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mHBAOParamsConstant, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

		mHBAOParams.FocalLen = D3DXVECTOR2(cameraProj._11, cameraProj._22);
		mHBAOParams.ClipInfo = D3DXVECTOR2(cameraProj._33, cameraProj._43);

		mHBAOParams.AOResolution = D3DXVECTOR2(viewport->Width, viewport->Height);
		mHBAOParams.InvAOResolution = D3DXVECTOR2(1.0f / viewport->Width, 1.0f / viewport->Height);

		mHBAOParams.RadiusSquared = mHBAOParams.Radius * mHBAOParams.Radius;
		mHBAOParams.InvRadiusSquared = 1.0f / mHBAOParams.RadiusSquared;
		mHBAOParams.MaxRadiusPixels = 0.1f * (std::min)(viewport->Width, viewport->Height);

		mHBAOParams.Strength = 1.0;

		memcpy(mappedResource.pData, &mHBAOParams, sizeof(mHBAOParams));

		d3dDeviceContext->Unmap(mHBAOParamsConstant, 0);
	}

	// Render full sreen quad
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mHBAOParamsConstant);

	ID3D11ShaderResourceView* srv[2] = { mDepthBuffer->GetShaderResourceView(), mHBAORandomSRV };
	d3dDeviceContext->PSSetShaderResources(0, 2, srv);
	ID3D11SamplerState* samplers[2] = { mPointClampSampler, mPointWarpSampler };
	d3dDeviceContext->PSSetSamplers(0, 2, samplers);
	d3dDeviceContext->PSSetShader(mHBAOPS->GetShader(), 0, 0);

	ID3D11RenderTargetView * renderTargets[1] = { mAOBuffer->GetRenderTargetView() };
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
	d3dDeviceContext->OMSetDepthStencilState(mDepthDisableState, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

	d3dDeviceContext->Draw(3, 0);

	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);

	//------------------------------------------------------------------------------------------------------
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mBlurParamsConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

		mBlurParams.CameraNear = viewerCamera.GetNearClip();
		mBlurParams.CameraFar = viewerCamera.GetFarClip();
		mBlurParams.InvResolution = D3DXVECTOR2(1.0f / viewport->Width, 1.0f / viewport->Height);

		float sigma = (mBlurParams.BlurRadius + 3) / 4;
		mBlurParams.BlurFalloff = 1.0f / (2 * sigma * sigma);

		mBlurParams.BlurSharpness = mBlurParams.BlurFalloff;

		//mBlurParams.BlurSharpness = (mBlurParams.BlurRadius + 1) / 2;

		memcpy(mappedResource.pData, &mBlurParams, sizeof(mBlurParams));
		
		d3dDeviceContext->Unmap(mBlurParamsConstants, 0);
	}

	// Blur X
	srv[1] = mAOBuffer->GetShaderResourceView(); 
	d3dDeviceContext->PSSetShaderResources(0, 2, srv);

	d3dDeviceContext->PSSetShader(mBlurXPS->GetShader(), 0, 0);
	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mBlurParamsConstants);

	renderTargets[0] = mBlurBuffer->GetRenderTargetView();
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
	d3dDeviceContext->Draw(3, 0);
	
	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);

	// Blur Y
	srv[1] = mBlurBuffer->GetShaderResourceView();
	d3dDeviceContext->PSSetShaderResources(0, 2, srv);
	d3dDeviceContext->PSSetShader(mBlurYPS->GetShader(), 0, 0);

	renderTargets[0] = mAOBuffer->GetRenderTargetView();
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
	d3dDeviceContext->Draw(3, 0);

	// Cleanup (aka make the runtime happy)
	d3dDeviceContext->VSSetShader(0, 0, 0);
	d3dDeviceContext->PSSetShader(0, 0, 0);
	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
	ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
	d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
	ID3D11Buffer* nullBuffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetConstantBuffers(0, 8, nullBuffer);
	d3dDeviceContext->PSSetConstantBuffers(0, 8, nullBuffer);
}

void SSAO::RenderAlchemyAO( ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
	d3dDeviceContext->ClearRenderTargetView(mAOBuffer->GetRenderTargetView(), zeros);

	const D3DXMATRIX& cameraProj = *viewerCamera.GetProjMatrix();

	// Fill AO constants
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mHBAOParamsConstant, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

		mHBAOParams.FocalLen = D3DXVECTOR2(cameraProj._11, cameraProj._22);
		mHBAOParams.ClipInfo = D3DXVECTOR2(cameraProj._33, cameraProj._43);

		mHBAOParams.AOResolution = D3DXVECTOR2(viewport->Width, viewport->Height);
		mHBAOParams.InvAOResolution = D3DXVECTOR2(1.0f / viewport->Width, 1.0f / viewport->Height);

		mHBAOParams.RadiusSquared = mHBAOParams.Radius * mHBAOParams.Radius;
		mHBAOParams.InvRadiusSquared = 1.0f / mHBAOParams.RadiusSquared;
		mHBAOParams.MaxRadiusPixels = 0.1f * (std::min)(viewport->Width, viewport->Height);

		mHBAOParams.Strength = 1.0;

		memcpy(mappedResource.pData, &mHBAOParams, sizeof(mHBAOParams));

		d3dDeviceContext->Unmap(mHBAOParamsConstant, 0);
	}

	// Render full sreen quad
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mHBAOParamsConstant);

	ID3D11ShaderResourceView* srv[2] = { mDepthBuffer->GetShaderResourceView(), mHBAORandomSRV };
	d3dDeviceContext->PSSetShaderResources(0, 2, srv);
	ID3D11SamplerState* samplers[2] = { mPointClampSampler, mPointWarpSampler };
	d3dDeviceContext->PSSetSamplers(0, 2, samplers);
	d3dDeviceContext->PSSetShader(mAlchemyAOPS->GetShader(), 0, 0);

	ID3D11RenderTargetView * renderTargets[1] = { mAOBuffer->GetRenderTargetView() };
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
	d3dDeviceContext->OMSetDepthStencilState(mDepthDisableState, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

	d3dDeviceContext->Draw(3, 0);

	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);

	//------------------------------------------------------------------------------------------------------
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mBlurParamsConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

		mBlurParams.CameraNear = viewerCamera.GetNearClip();
		mBlurParams.CameraFar = viewerCamera.GetFarClip();
		mBlurParams.InvResolution = D3DXVECTOR2(1.0f / viewport->Width, 1.0f / viewport->Height);

		float sigma = (mBlurParams.BlurRadius + 3) / 4;
		mBlurParams.BlurFalloff = 1.0f / (2 * sigma * sigma);

		mBlurParams.BlurSharpness = mBlurParams.BlurFalloff;

		//mBlurParams.BlurSharpness = (mBlurParams.BlurRadius + 1) / 2;

		memcpy(mappedResource.pData, &mBlurParams, sizeof(mBlurParams));

		d3dDeviceContext->Unmap(mBlurParamsConstants, 0);
	}

	// Blur X
	srv[1] = mAOBuffer->GetShaderResourceView(); 
	d3dDeviceContext->PSSetShaderResources(0, 2, srv);

	d3dDeviceContext->PSSetShader(mBlurXPS->GetShader(), 0, 0);
	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mBlurParamsConstants);

	renderTargets[0] = mBlurBuffer->GetRenderTargetView();
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
	d3dDeviceContext->Draw(3, 0);

	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);

	// Blur Y
	srv[1] = mBlurBuffer->GetShaderResourceView();
	d3dDeviceContext->PSSetShaderResources(0, 2, srv);
	d3dDeviceContext->PSSetShader(mBlurYPS->GetShader(), 0, 0);

	renderTargets[0] = mAOBuffer->GetRenderTargetView();
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
	d3dDeviceContext->Draw(3, 0);

	// Cleanup (aka make the runtime happy)
	d3dDeviceContext->VSSetShader(0, 0, 0);
	d3dDeviceContext->PSSetShader(0, 0, 0);
	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
	ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
	d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
	ID3D11Buffer* nullBuffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetConstantBuffers(0, 8, nullBuffer);
	d3dDeviceContext->PSSetConstantBuffers(0, 8, nullBuffer);

}

void SSAO::ComputeShading( ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	std::shared_ptr<Texture2D> &accumulateBuffer = mLightPrePass ? mLightAccumulateBuffer : mLitBuffer;

	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
	d3dDeviceContext->ClearRenderTargetView(accumulateBuffer->GetRenderTargetView(), zeros);

	if (!mLightPrePass) // Deferred shading
	{
		// Set GBuffer, all light type needs it
		ID3D11ShaderResourceView* srv[] = { mGBufferSRV[0], mGBufferSRV[1],
			                                mDepthBuffer->GetShaderResourceView(),
											mAOBuffer->GetShaderResourceView() 
		};

		d3dDeviceContext->PSSetShaderResources(0, ARRAYSIZE(srv), srv);
	}
	else
	{
		// Set GBuffer, all light type needs it
		ID3D11ShaderResourceView* srv[] = { mGBufferSRV[0],  // Normal
			                                mDepthBuffer->GetShaderResourceView() // Depth
		                                  };
		d3dDeviceContext->PSSetShaderResources(0, ARRAYSIZE(srv), srv);
	}

	d3dDeviceContext->PSSetSamplers(0, 1, &mPointClampSampler);
	d3dDeviceContext->RSSetViewports(1, viewport);

	ID3D11RenderTargetView * renderTargets[1] = { accumulateBuffer->GetRenderTargetView() };
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBufferReadOnlyDSV);
	d3dDeviceContext->OMSetBlendState(mLightingBlendState, 0, 0xFFFFFFFF);

	//DrawPointLight(d3dDeviceContext, lights, viewerCamera);
	DrawDirectionalLight(d3dDeviceContext, lights, viewerCamera);

	if (mLightPrePass)
	{
		// final shading pass
		const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
		d3dDeviceContext->ClearRenderTargetView(mLitBuffer->GetRenderTargetView(), zeros);

		d3dDeviceContext->IASetInputLayout(0);
		d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

		// Fill inverse projection
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mPerObjectConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			PerObjectConstants* constants = static_cast<PerObjectConstants *>(mappedResource.pData);

			// WorldView stores InvProj
			D3DXMatrixInverse(&constants->WorldView, NULL, viewerCamera.GetProjMatrix());

			d3dDeviceContext->Unmap(mPerObjectConstants, 0);
		}

		d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerObjectConstants);	
		d3dDeviceContext->VSSetShader(mDeferredShadingVS[LT_DirectionalLigt]->GetShader(), 0, 0);

		ID3D11RenderTargetView * renderTargets[1] = { mLitBuffer->GetRenderTargetView() };
		d3dDeviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
		d3dDeviceContext->OMSetDepthStencilState(mDepthDisableState, 0);

		// Set GBuffer, all light type needs it
		ID3D11ShaderResourceView* srv[] = { mGBufferSRV[0], mGBufferSRV[1],
			mLightAccumulateBuffer->GetShaderResourceView(),
			mAOBuffer->GetShaderResourceView() 
		};

		d3dDeviceContext->PSSetShaderResources(0, ARRAYSIZE(srv), srv);
		d3dDeviceContext->PSSetSamplers(0, 1, &mPointClampSampler);
		d3dDeviceContext->PSSetShader(mDeferredLightingShadingPS->GetShader(), 0, 0);

		d3dDeviceContext->RSSetState(mRasterizerState);
	
		d3dDeviceContext->Draw(3, 0);
	}

	/*if (mCullTechnique == Cull_Deferred_Quad)
	{
		d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
		DrawLightVolumeDebug(d3dDeviceContext, lights, viewerCamera);
	}*/

	// Cleanup (aka make the runtime happy)
	d3dDeviceContext->VSSetShader(0, 0, 0);
	d3dDeviceContext->PSSetShader(0, 0, 0);
	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
	ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
	d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
	ID3D11Buffer* nullBuffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetConstantBuffers(0, 8, nullBuffer);
	d3dDeviceContext->PSSetConstantBuffers(0, 8, nullBuffer);
}

void SSAO::DrawDirectionalLight( ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera )
{
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	// Fill inverse projection
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mPerObjectConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		PerObjectConstants* constants = static_cast<PerObjectConstants *>(mappedResource.pData);

		// WorldView stores InvProj
		D3DXMatrixInverse(&constants->WorldView, NULL, viewerCamera.GetProjMatrix());

		d3dDeviceContext->Unmap(mPerObjectConstants, 0);
	}

	d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerObjectConstants);	
	d3dDeviceContext->VSSetShader(mDeferredShadingVS[LT_DirectionalLigt]->GetShader(), 0, 0);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetConstantBuffers(1, 1, &mLightConstants);
	d3dDeviceContext->PSSetShader(
		mLightPrePass ? mDeferredLightingPS[LT_DirectionalLigt]->GetShader() : mDeferredShadingPS[LT_DirectionalLigt]->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->OMSetDepthStencilState(mDepthDisableState, 0);

	// Find first directional light index
	size_t idx = 0;
	while(idx < lights.mLights.size() && lights.mLights[idx].LightType != LT_DirectionalLigt) idx++;
	
	while(idx < lights.mLights.size() && lights.mLights[idx].LightType == LT_DirectionalLigt)
	{
		// Fill DirectionalLight constants
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mLightConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		LightCBuffer* lightCBffer = static_cast<LightCBuffer*>(mappedResource.pData);

		D3DXVec3TransformNormal(&lightCBffer->LightDirection, &lights.mLights[idx].LightDirection, viewerCamera.GetViewMatrix());
		D3DXVec3Normalize(&lightCBffer->LightDirection, &lightCBffer->LightDirection);
		lightCBffer->LightColor = lights.mLights[idx].LightColor;
		d3dDeviceContext->Unmap(mLightConstants, 0);

		d3dDeviceContext->Draw(3, 0);

		idx++;
	}	
}

void SSAO::DrawPointLight( ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera )
{
	const D3DXMATRIX& cameraProj = *viewerCamera.GetProjMatrix();
	const D3DXMATRIX& cameraView = *viewerCamera.GetViewMatrix();

	bool useScreenQuad = (mCullTechnique == Cull_Deferred_Quad);
	D3DXVECTOR4 bound;

	// GPU Screen Quad
	if (useScreenQuad)
	{
		d3dDeviceContext->IASetInputLayout(0);
		d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);
		d3dDeviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

		d3dDeviceContext->VSSetShader(mScreenQuadVS->GetShader(), 0, 0);
		d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);	
		d3dDeviceContext->VSSetConstantBuffers(1, 1, &mLightConstants);	

		d3dDeviceContext->GSSetConstantBuffers(0, 1, &mPerFrameConstants);	
		d3dDeviceContext->GSSetShader(mScreenQuadGS->GetShader(), 0, 0);

		//UINT offset = 0;
		//d3dDeviceContext->SOSetTargets(1, &mStreamOutputGPU, &offset);
	}
	else
	{
		d3dDeviceContext->IASetInputLayout(mLightProxyVertexLayout);
		d3dDeviceContext->VSSetShader(mDeferredShadingVS[LT_PointLight]->GetShader(), 0, 0);
		d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerObjectConstants);	
	}

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetConstantBuffers(1, 1, &mLightConstants);		
	d3dDeviceContext->PSSetShader(
		mLightPrePass ? mDeferredLightingPS[LT_PointLight]->GetShader() : mDeferredShadingPS[LT_PointLight]->GetShader(), 0, 0);

	size_t idx = 0;
	while(idx < lights.mLights.size() && lights.mLights[idx].LightType != LT_PointLight) idx++;

	while(idx < lights.mLights.size() && lights.mLights[idx].LightType == LT_PointLight)
	{
		const LightAnimation::Light& light = lights.mLights[idx];	

		float lightRadius = light.LightAttenuation.y;  // Attenuation End
		const D3DXVECTOR3& lightPosition = light.LightPosition;

		D3DXMATRIX world;
		D3DXMatrixScaling(&world, lightRadius, lightRadius, lightRadius);   // Scale
		world._41 = lightPosition.x;
		world._42 = lightPosition.y;
		world._43 = lightPosition.z;         // translation

		// Fill per object constants
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mPerObjectConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			PerObjectConstants* constants = static_cast<PerObjectConstants *>(mappedResource.pData);

			// Only need those two
			constants->WorldView = world * cameraView;
			constants->WorldViewProj = constants->WorldView * cameraProj;

			d3dDeviceContext->Unmap(mPerObjectConstants, 0);
		}
	
		// Fill PointLight constants
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mLightConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			LightCBuffer* lightCBuffer = static_cast<LightCBuffer*>(mappedResource.pData);

			D3DXVec3TransformCoord(&lightCBuffer->LightPosition, &lightPosition, &cameraView);
			lightCBuffer->LightColor = light.LightColor;
			lightCBuffer->LightAttenuation = light.LightAttenuation;

			d3dDeviceContext->Unmap(mLightConstants, 0);
		}

		if (useScreenQuad)
		{
			d3dDeviceContext->OMSetDepthStencilState(mDepthLEQualState, 0);
			d3dDeviceContext->RSSetState(mRasterizerState);
			d3dDeviceContext->Draw(1, 0);
		}
		else
		{
			D3DXVECTOR3 cameraToLight = light.LightPosition - *viewerCamera.GetEyePt();
			if (D3DXVec3Length(&cameraToLight) < lightRadius)
			{
				// Camera inside light volume, draw backfaces, Cull front
				d3dDeviceContext->OMSetDepthStencilState(mDepthGreaterState, 0);
				d3dDeviceContext->RSSetState(mRasterizerFrontState);
			}
			else
			{
				// Camera outside light volume, draw front, Cull back
				d3dDeviceContext->OMSetDepthStencilState(mDepthLEQualState, 0);
				d3dDeviceContext->RSSetState(mRasterizerState);
			}

			mPointLightProxy->Render(d3dDeviceContext);
		}
		
		idx++;
	}	

	d3dDeviceContext->GSSetShader(0, 0, 0);

	//d3dDeviceContext->CopyResource( mStreamOutputCPU, mStreamOutputGPU );

	//D3D11_MAPPED_SUBRESOURCE data;
	//if( SUCCEEDED( d3dDeviceContext->Map( mStreamOutputCPU, 0, D3D11_MAP_READ, 0, &data ) ) )
	//{
	//	struct GS_OUTPUT
	//	{
	//		D3DXVECTOR3 Tex;
	//		D3DXVECTOR3 ViewRay;
	//		D3DXVECTOR4 PosCS;
	//	};

	//	auto sz = sizeof GS_OUTPUT;

	//	GS_OUTPUT pRaw[4];
	//	
	//	memcpy(pRaw, data.pData, sizeof(pRaw));
	//	
	//	/* Work with the pRaw[] array here */
	//	// Consider StringCchPrintf() and OutputDebugString() as simple ways of printing the above struct, or use the debugger and step through.

	//	d3dDeviceContext->Unmap( mStreamOutputCPU, 0 );
	//}

}

void SSAO::DrawLightVolumeDebug( ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera )
{
	const D3DXMATRIX& cameraProj = *viewerCamera.GetProjMatrix();
	const D3DXMATRIX& cameraView = *viewerCamera.GetViewMatrix();

	d3dDeviceContext->IASetInputLayout(mLightProxyVertexLayout);

	d3dDeviceContext->VSSetShader(mDebugVS->GetShader(), 0, 0);
	d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);	

	d3dDeviceContext->PSSetShader(mDebugPS->GetShader(), 0, 0);	
	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mLightConstants);

	d3dDeviceContext->OMSetDepthStencilState(mDepthLEQualState, 0);
	d3dDeviceContext->RSSetState(mRasterizerState);

	size_t idx = 0;
	while(idx < lights.mLights.size() && lights.mLights[idx].LightType != LT_PointLight) idx++;

	while(idx < lights.mLights.size() && lights.mLights[idx].LightType == LT_PointLight)
	{
		const LightAnimation::Light& light = lights.mLights[idx];

		float lightRadius = light.LightAttenuation.y /*/ 10.0f*/;  // Attenuation End
		const D3DXVECTOR3& lightPosition = light.LightPosition;

		D3DXMATRIX world;
		D3DXMatrixScaling(&world, lightRadius, lightRadius, lightRadius);   // Scale
		world._41 = lightPosition.x;
		world._42 = lightPosition.y;
		world._43 = lightPosition.z;         // translation

		// Fill per frame constants
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			PerObjectConstants* constants = static_cast<PerObjectConstants *>(mappedResource.pData);

			// Only need those two
			constants->WorldView = world * cameraView;
			constants->WorldViewProj = constants->WorldView * cameraProj;

			d3dDeviceContext->Unmap(mPerFrameConstants, 0);
		}

		// Fill PointLight constants
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mLightConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			LightCBuffer* lightCBuffer = static_cast<LightCBuffer*>(mappedResource.pData);

			D3DXVec3TransformCoord(&lightCBuffer->LightPosition, &lightPosition, &cameraView);
			lightCBuffer->LightColor = light.LightColor;
			lightCBuffer->LightAttenuation = light.LightAttenuation;

			d3dDeviceContext->Unmap(mLightConstants, 0);
		}

		mPointLightProxy->Render(d3dDeviceContext);

		idx++;
	}	
}

void SSAO::EdgeAA( ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, const D3D11_VIEWPORT* viewport )
{
	// Render full sreen quad
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	// GBuffer normal and depth
	ID3D11ShaderResourceView* srv[] = { 
		mGBufferSRV.front(), // Normal
		mGBufferSRV.back(),  // Depth
		mLitBuffer->GetShaderResourceView() 
	};

	d3dDeviceContext->PSSetShaderResources(0, 3, srv);
	d3dDeviceContext->PSSetSamplers(0, 1, &mPointClampSampler);
	d3dDeviceContext->PSSetSamplers(1, 1, &mDiffuseSampler);

	d3dDeviceContext->PSSetShader(mEdgeAAPS->GetShader(), 0, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
	d3dDeviceContext->OMSetDepthStencilState(mDepthDisableState, 0);
	d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, NULL);

	d3dDeviceContext->Draw(3, 0);
}

void SSAO::PostProcess( ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth, const D3D11_VIEWPORT* viewport )
{
	//EdgeAA(d3dDeviceContext, backBuffer, viewport);

	// Render full sreen quad
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	// GBuffer normal and depth
	ID3D11ShaderResourceView* srv[] = { mShowAO ? mAOBuffer->GetShaderResourceView() : mLitBuffer->GetShaderResourceView() };
	d3dDeviceContext->PSSetShaderResources(0, 1, srv);
	d3dDeviceContext->PSSetSamplers(0, 1, &mPointClampSampler);

	d3dDeviceContext->PSSetShader(mShowAO ? mFullQuadSpriteAO->GetShader() : mFullQuadSprite->GetShader(), 0, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
	d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, backDepth);

	d3dDeviceContext->Draw(3, 0);
}

void SSAO::CreateHBAORandomTexture(ID3D11Device* pD3DDevice)
{
	//std::mt19937 eng; // Mersenne Twister
	//std::uniform_real_distribution<float> dist(0.0f, 1.0f);

	const int RANDOM_TEXTURE_WIDTH = 4;
	const int NUM_DIRECTIONS = 8;

	//uint16_t *data = new uint16_t[RANDOM_TEXTURE_WIDTH*RANDOM_TEXTURE_WIDTH*4];
	//for (UINT i = 0; i < RANDOM_TEXTURE_WIDTH*RANDOM_TEXTURE_WIDTH*4; i += 4)
	//{
	//	float r1 = dist(eng);
	//	float r2 = dist(eng);

	//	// Use random rotatation angles in [0,2P/NUM_HBAO_DIRECTIONS).
	//	// This looks the same as sampling [0,2PI), but is faster.
	//	float angle = 2.0f * D3DX_PI * r1 / NUM_DIRECTIONS;
	//	data[i  ] = (uint16_t)((1<<15)*cos(angle));
	//	data[i+1] = (uint16_t)((1<<15)*sin(angle));
	//	data[i+2] = (uint16_t)((1<<15)*r2);
	//	data[i+3] = 0;
	//}

	// Mersenne-Twister random numbers in [0,1).
	static const float MersenneTwisterNumbers[1024] = {
		0.463937f,0.340042f,0.223035f,0.468465f,0.322224f,0.979269f,0.031798f,0.973392f,0.778313f,0.456168f,0.258593f,0.330083f,0.387332f,0.380117f,0.179842f,0.910755f,
		0.511623f,0.092933f,0.180794f,0.620153f,0.101348f,0.556342f,0.642479f,0.442008f,0.215115f,0.475218f,0.157357f,0.568868f,0.501241f,0.629229f,0.699218f,0.707733f,
		0.556725f,0.005520f,0.708315f,0.583199f,0.236644f,0.992380f,0.981091f,0.119804f,0.510866f,0.560499f,0.961497f,0.557862f,0.539955f,0.332871f,0.417807f,0.920779f,
		0.730747f,0.076690f,0.008562f,0.660104f,0.428921f,0.511342f,0.587871f,0.906406f,0.437980f,0.620309f,0.062196f,0.119485f,0.235646f,0.795892f,0.044437f,0.617311f,
		0.891128f,0.263161f,0.245298f,0.276518f,0.786986f,0.059768f,0.424345f,0.433341f,0.052190f,0.699924f,0.139479f,0.402873f,0.741976f,0.557978f,0.127093f,0.946352f,
		0.205587f,0.092822f,0.422956f,0.715176f,0.711952f,0.926062f,0.368646f,0.286516f,0.241413f,0.831616f,0.232247f,0.478637f,0.366948f,0.432024f,0.268430f,0.619122f,
		0.391737f,0.056698f,0.067702f,0.509009f,0.920858f,0.298358f,0.701015f,0.044309f,0.936794f,0.485976f,0.271286f,0.108779f,0.325844f,0.682314f,0.955090f,0.658145f,
		0.295861f,0.562559f,0.867194f,0.810552f,0.487959f,0.869567f,0.224706f,0.962637f,0.646548f,0.003730f,0.228857f,0.263667f,0.365176f,0.958302f,0.606619f,0.901869f,
		0.757257f,0.306061f,0.633172f,0.407697f,0.443632f,0.979959f,0.922944f,0.946421f,0.594079f,0.604343f,0.864211f,0.187557f,0.877119f,0.792025f,0.954840f,0.976719f,
		0.350546f,0.834781f,0.945113f,0.155877f,0.411841f,0.552378f,0.855409f,0.741383f,0.761251f,0.896223f,0.782077f,0.266224f,0.128873f,0.645733f,0.591567f,0.247385f,
		0.260848f,0.811970f,0.653369f,0.976713f,0.221533f,0.957436f,0.294018f,0.159025f,0.820596f,0.569601f,0.934328f,0.467182f,0.763165f,0.835736f,0.240033f,0.389869f,
		0.998754f,0.783739f,0.758034f,0.614317f,0.221128f,0.502497f,0.978066f,0.247794f,0.619551f,0.658307f,0.769667f,0.768478f,0.337143f,0.370689f,0.084723f,0.510534f,
		0.594996f,0.994636f,0.181230f,0.868113f,0.312023f,0.480495f,0.177356f,0.367374f,0.741642f,0.202983f,0.229404f,0.108165f,0.098607f,0.010412f,0.727391f,0.942217f,
		0.023850f,0.110631f,0.958293f,0.208996f,0.584609f,0.491803f,0.238266f,0.591587f,0.297477f,0.681421f,0.215040f,0.587764f,0.704494f,0.978978f,0.911686f,0.692657f,
		0.462987f,0.273259f,0.802855f,0.651633f,0.736728f,0.986217f,0.402363f,0.524098f,0.740470f,0.799076f,0.918257f,0.705367f,0.477477f,0.102279f,0.809959f,0.860645f,
		0.118276f,0.009567f,0.280106f,0.948473f,0.025423f,0.458173f,0.512607f,0.082088f,0.536906f,0.472590f,0.835726f,0.078518f,0.357919f,0.797522f,0.570516f,0.162719f,
		0.815968f,0.874141f,0.915300f,0.392073f,0.366307f,0.766238f,0.462755f,0.087614f,0.402357f,0.277686f,0.294194f,0.392791f,0.504893f,0.263420f,0.509197f,0.518974f,
		0.738809f,0.965800f,0.003864f,0.976899f,0.292287f,0.837148f,0.525498f,0.743779f,0.359015f,0.060636f,0.595481f,0.483102f,0.900195f,0.423277f,0.981990f,0.154968f,
		0.085584f,0.681517f,0.814437f,0.105936f,0.972238f,0.207062f,0.994642f,0.989271f,0.646217f,0.330263f,0.432094f,0.139929f,0.908629f,0.271571f,0.539319f,0.845182f,
		0.140069f,0.001406f,0.340195f,0.582218f,0.693570f,0.293148f,0.733441f,0.375523f,0.676068f,0.130642f,0.606523f,0.441091f,0.113519f,0.844462f,0.399921f,0.551049f,
		0.482781f,0.894854f,0.188909f,0.431045f,0.043693f,0.394601f,0.544309f,0.798761f,0.040417f,0.022292f,0.681257f,0.598379f,0.069981f,0.255632f,0.174776f,0.880842f,
		0.412071f,0.397976f,0.932835f,0.979471f,0.244276f,0.488083f,0.313785f,0.858199f,0.390958f,0.426132f,0.754800f,0.360781f,0.862827f,0.526424f,0.090054f,0.673971f,
		0.715044f,0.237489f,0.210234f,0.952837f,0.448429f,0.738062f,0.077342f,0.260666f,0.590478f,0.127519f,0.628981f,0.136232f,0.860189f,0.596789f,0.524043f,0.897171f,
		0.648864f,0.116735f,0.666835f,0.536993f,0.811733f,0.854961f,0.857206f,0.945069f,0.434195f,0.602343f,0.823780f,0.109481f,0.684652f,0.195598f,0.213630f,0.283516f,
		0.387092f,0.182029f,0.834655f,0.948975f,0.373107f,0.249751f,0.162575f,0.587850f,0.192648f,0.737863f,0.777432f,0.651490f,0.562558f,0.918301f,0.094830f,0.260698f,
		0.629400f,0.751325f,0.362210f,0.649610f,0.397390f,0.670624f,0.215662f,0.925465f,0.908397f,0.486853f,0.141060f,0.236122f,0.926399f,0.416056f,0.781483f,0.538538f,
		0.119521f,0.004196f,0.847561f,0.876772f,0.945552f,0.935095f,0.422025f,0.502860f,0.932500f,0.116670f,0.700854f,0.995577f,0.334925f,0.174659f,0.982878f,0.174110f,
		0.734294f,0.769366f,0.917586f,0.382623f,0.795816f,0.051831f,0.528121f,0.691978f,0.337981f,0.675601f,0.969444f,0.354908f,0.054569f,0.254278f,0.978879f,0.611259f,
		0.890006f,0.712659f,0.219624f,0.826455f,0.351117f,0.087383f,0.862534f,0.805461f,0.499343f,0.482118f,0.036473f,0.815656f,0.016539f,0.875982f,0.308313f,0.650039f,
		0.494165f,0.615983f,0.396761f,0.921652f,0.164612f,0.472705f,0.559820f,0.675677f,0.059891f,0.295793f,0.818010f,0.769365f,0.158699f,0.648142f,0.228793f,0.627454f,
		0.138543f,0.639463f,0.200399f,0.352380f,0.470716f,0.888694f,0.311777f,0.571183f,0.979317f,0.457287f,0.115151f,0.725631f,0.620539f,0.629373f,0.850207f,0.949974f,
		0.254675f,0.142306f,0.688887f,0.307235f,0.284882f,0.847675f,0.617070f,0.207422f,0.550545f,0.541886f,0.173878f,0.474841f,0.678372f,0.289180f,0.528111f,0.306538f,
		0.869399f,0.040299f,0.417301f,0.472569f,0.857612f,0.917462f,0.842319f,0.986865f,0.604528f,0.731115f,0.607880f,0.904675f,0.397955f,0.627867f,0.533371f,0.656758f,
		0.627210f,0.223554f,0.268442f,0.254858f,0.834380f,0.131010f,0.838028f,0.613512f,0.821627f,0.859779f,0.405212f,0.909901f,0.036186f,0.643093f,0.187064f,0.945730f,
		0.319022f,0.709012f,0.852200f,0.559587f,0.865751f,0.368890f,0.840416f,0.950571f,0.315120f,0.331749f,0.509218f,0.468617f,0.119006f,0.541820f,0.983444f,0.115515f,
		0.299804f,0.840386f,0.445282f,0.900755f,0.633600f,0.304196f,0.996153f,0.844025f,0.462361f,0.314402f,0.850035f,0.773624f,0.958303f,0.765382f,0.567577f,0.722607f,
		0.001299f,0.189690f,0.364661f,0.192390f,0.836882f,0.783680f,0.026723f,0.065230f,0.588791f,0.937752f,0.993644f,0.597499f,0.851975f,0.670339f,0.360987f,0.755649f,
		0.571521f,0.231990f,0.425067f,0.116442f,0.321815f,0.629616f,0.701207f,0.716931f,0.146357f,0.360526f,0.498487f,0.846096f,0.307994f,0.323456f,0.288884f,0.477935f,
		0.236433f,0.876589f,0.667459f,0.977175f,0.179347f,0.479408f,0.633292f,0.957666f,0.343651f,0.871846f,0.452856f,0.895494f,0.327657f,0.867779f,0.596825f,0.907009f,
		0.417409f,0.530739f,0.547422f,0.141032f,0.721096f,0.587663f,0.830054f,0.460860f,0.563898f,0.673780f,0.035824f,0.755808f,0.331846f,0.653460f,0.926339f,0.724599f,
		0.978501f,0.495221f,0.098108f,0.936766f,0.139911f,0.851336f,0.889867f,0.376509f,0.661482f,0.156487f,0.671886f,0.487835f,0.046571f,0.441975f,0.014015f,0.440433f,
		0.235927f,0.163762f,0.075399f,0.254734f,0.214011f,0.554803f,0.712877f,0.795785f,0.471616f,0.105032f,0.355989f,0.834418f,0.498021f,0.018318f,0.364799f,0.918869f,
		0.909222f,0.858506f,0.928250f,0.946347f,0.755364f,0.408753f,0.137841f,0.247870f,0.300618f,0.470068f,0.248714f,0.521691f,0.009862f,0.891550f,0.908914f,0.227533f,
		0.702908f,0.596738f,0.581597f,0.099904f,0.804893f,0.947457f,0.080649f,0.375755f,0.890498f,0.689130f,0.600941f,0.382261f,0.814084f,0.258373f,0.278029f,0.907399f,
		0.625024f,0.016637f,0.502896f,0.743077f,0.247834f,0.846201f,0.647815f,0.379888f,0.517357f,0.921494f,0.904846f,0.805645f,0.671974f,0.487205f,0.678009f,0.575624f,
		0.910779f,0.947642f,0.524788f,0.231298f,0.299029f,0.068158f,0.569690f,0.121049f,0.701641f,0.311914f,0.447310f,0.014019f,0.013391f,0.257855f,0.481835f,0.808870f,
		0.628222f,0.780253f,0.202719f,0.024902f,0.774355f,0.783080f,0.330077f,0.788864f,0.346888f,0.778702f,0.261985f,0.696691f,0.212839f,0.713849f,0.871828f,0.639753f,
		0.711037f,0.651247f,0.042374f,0.236938f,0.746267f,0.235043f,0.442707f,0.195417f,0.175918f,0.987980f,0.031270f,0.975425f,0.277087f,0.752667f,0.639751f,0.507857f,
		0.873571f,0.775393f,0.390003f,0.415997f,0.287861f,0.189340f,0.837939f,0.186253f,0.355633f,0.803788f,0.029124f,0.802046f,0.248046f,0.354010f,0.420571f,0.109523f,
		0.731250f,0.700653f,0.716019f,0.651507f,0.250055f,0.884214f,0.364255f,0.244975f,0.472268f,0.080641f,0.309332f,0.250613f,0.519091f,0.066142f,0.037804f,0.865752f,
		0.767738f,0.617325f,0.537048f,0.743959f,0.401200f,0.595458f,0.869843f,0.193999f,0.670364f,0.018494f,0.743159f,0.979555f,0.382352f,0.191059f,0.992247f,0.946175f,
		0.306473f,0.793720f,0.687331f,0.556239f,0.958367f,0.390949f,0.357823f,0.110213f,0.977540f,0.831431f,0.485895f,0.148678f,0.847327f,0.733145f,0.397393f,0.376365f,
		0.398704f,0.463869f,0.976946f,0.844771f,0.075688f,0.473865f,0.470958f,0.548172f,0.350174f,0.727441f,0.123139f,0.347760f,0.839587f,0.562705f,0.036853f,0.564723f,
		0.960356f,0.220534f,0.906969f,0.677664f,0.841052f,0.111530f,0.032346f,0.027749f,0.468255f,0.229196f,0.508756f,0.199613f,0.298103f,0.677274f,0.526005f,0.828221f,
		0.413321f,0.305165f,0.223361f,0.778072f,0.198089f,0.414976f,0.007498f,0.464238f,0.785213f,0.534428f,0.060537f,0.572427f,0.693334f,0.865843f,0.034964f,0.586806f,
		0.161710f,0.203743f,0.656513f,0.604340f,0.688333f,0.257211f,0.246437f,0.338237f,0.839947f,0.268420f,0.913245f,0.759551f,0.289283f,0.347280f,0.508970f,0.361526f,
		0.554649f,0.086439f,0.024344f,0.661653f,0.988840f,0.110613f,0.129422f,0.405940f,0.781764f,0.303922f,0.521807f,0.236282f,0.277927f,0.699228f,0.733812f,0.772090f,
		0.658423f,0.056394f,0.153089f,0.536837f,0.792251f,0.165229f,0.592251f,0.228337f,0.147078f,0.116056f,0.319268f,0.293400f,0.872600f,0.842240f,0.306238f,0.228790f,
		0.745704f,0.821321f,0.778268f,0.611390f,0.969139f,0.297654f,0.367369f,0.815074f,0.985840f,0.693232f,0.411759f,0.366651f,0.345481f,0.609060f,0.778929f,0.640823f,
		0.340969f,0.328489f,0.898686f,0.952345f,0.272572f,0.758995f,0.111269f,0.613403f,0.864397f,0.607601f,0.357317f,0.227619f,0.177081f,0.773828f,0.318257f,0.298335f,
		0.679382f,0.454625f,0.976745f,0.244511f,0.880111f,0.046238f,0.451342f,0.709265f,0.784123f,0.488338f,0.228713f,0.041251f,0.077453f,0.718891f,0.454221f,0.039182f,
		0.614777f,0.538681f,0.856650f,0.888921f,0.184013f,0.487999f,0.880338f,0.726824f,0.112945f,0.835710f,0.943366f,0.340094f,0.167909f,0.241240f,0.125953f,0.460130f,
		0.789923f,0.313898f,0.640780f,0.795920f,0.198025f,0.407344f,0.673839f,0.414326f,0.185900f,0.353436f,0.786795f,0.422102f,0.133975f,0.363270f,0.393833f,0.748760f,
		0.328130f,0.115681f,0.253865f,0.526924f,0.672761f,0.517447f,0.686442f,0.532847f,0.551176f,0.667406f,0.382640f,0.408796f,0.649460f,0.613948f,0.600470f,0.485404f,
	};

	int randomNumberIndex = 0;
	uint16_t *data = new uint16_t[RANDOM_TEXTURE_WIDTH*RANDOM_TEXTURE_WIDTH*4];
	for (UINT i = 0; i < RANDOM_TEXTURE_WIDTH*RANDOM_TEXTURE_WIDTH*4; i += 4)
	{
		assert(randomNumberIndex < 1024);
		float r1 = MersenneTwisterNumbers[randomNumberIndex++];
		float r2 = MersenneTwisterNumbers[randomNumberIndex++];

		// Use random rotatation angles in [0,2P/NUM_HBAO_DIRECTIONS).
		// This looks the same as sampling [0,2PI), but is faster.
		float angle = 2.0f * D3DX_PI * r1 / NUM_DIRECTIONS;
		data[i  ] = (uint16_t)((1<<15)*cos(angle));
		data[i+1] = (uint16_t)((1<<15)*sin(angle));
		data[i+2] = (uint16_t)((1<<15)*r2);
		data[i+3] = 0;
	}

	D3D11_TEXTURE2D_DESC desc;
	desc.Width            = RANDOM_TEXTURE_WIDTH;
	desc.Height           = RANDOM_TEXTURE_WIDTH;
	desc.MipLevels        = 1;
	desc.ArraySize        = 1;
	desc.Format           = DXGI_FORMAT_R16G16B16A16_SNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage            = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags   = 0;
	desc.MiscFlags        = 0;

	D3D11_SUBRESOURCE_DATA srd;
	srd.pSysMem          = data;
	srd.SysMemPitch      = RANDOM_TEXTURE_WIDTH*4*sizeof(uint16_t);
	srd.SysMemSlicePitch = 0;

	SAFE_RELEASE(mHBAORandomTexture);
	pD3DDevice->CreateTexture2D(&desc, &srd, &mHBAORandomTexture);

	SAFE_RELEASE(mHBAORandomSRV);
	pD3DDevice->CreateShaderResourceView(mHBAORandomTexture, NULL, &mHBAORandomSRV);

	delete[] data;

}



