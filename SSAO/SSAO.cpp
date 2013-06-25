#include "DXUT.h"
#include "SSAO.h"
#include "Texture2D.h"
#include "Shader.h"
#include "DXUTcamera.h"
#include "SDKMesh.h"
#include "LightAnimation.h"
#include "Scene.h"

#include "Utility.h"

// NOTE: Must match layout of shader constant buffers

__declspec(align(16))
struct PerFrameConstants
{
	D3DXMATRIX Proj;
	D3DXMATRIX InvProj;
	D3DXVECTOR4 NearFar;
};

__declspec(align(16))
struct PerObjectConstants
{
	D3DXMATRIX WorldView;
	D3DXMATRIX WorldViewProj;
};

__declspec(align(16))
struct SSAOParams
{
	D3DXVECTOR4 ViewParams;
	D3DXVECTOR4 AOParams;
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
	: mDepthBufferReadOnlyDSV(0), mLighting(false)
{
	mFullQuadSprite = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\FullQuadSprite.hlsl", "FullQuadSpritePS", nullptr);

	mGBufferVS = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\GBuffer.hlsl", "GBufferVS", nullptr);
	mGBufferPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\GBuffer.hlsl", "GBufferPS", nullptr);

	mFullScreenTriangleVS = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\FullScreenTriangle.hlsl", "FullScreenTriangleVS", nullptr);
	//mSSAOCrytekPS = std::make_shared<PixelShader>(d3dDevice, L".\\Media\\Shaders\\SSAO.hlsl", "SSAOPS", nullptr);

	mEdgeAAPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\EdgeDetect.hlsl", "DL_GetEdgeWeight", nullptr);

	mForwardDirectionalVS = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\ForwardRendering.hlsl", "ForwardVS", nullptr);
	mForwardDirectionalPS = ShaderFactory::CreateShader<PixelShader>(d3dDevice, L".\\Media\\Shaders\\ForwardRendering.hlsl", "ForwardPS", nullptr);
	
	mScreenQuadVS = ShaderFactory::CreateShader<VertexShader>(d3dDevice, L".\\Media\\Shaders\\GPUScreenQuad.hlsl", "GPUQuadVS", nullptr);
	mScreenQuadGS = ShaderFactory::CreateShader<GeometryShader>(d3dDevice, L".\\Media\\Shaders\\GPUScreenQuad.hlsl", "GPUQuadGS", nullptr);

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
		d3dDevice->CreateSamplerState(&desc, &mNoiseSampler);
		DXUT_SetDebugName(mNoiseSampler, "mNoiseSampler");
	}

	// Create sampler for GBuffer
	{
		CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
		desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		d3dDevice->CreateSamplerState(&desc, &mGBufferSampler);	
		DXUT_SetDebugName(mGBufferSampler, "mGBufferSampler");
	}

	// Create constant buffer
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
		CD3D11_BUFFER_DESC desc(sizeof(LightCBuffer), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		d3dDevice->CreateBuffer(&desc, nullptr, &mLightConstants);
		DXUT_SetDebugName(mLightConstants, "mPointLightConstants");
	}

	// Load light volume mesh 
	{
		HRESULT hr;

		mPointLightProxy = new CDXUTSDKMesh;
		mSpotLightProxy = new CDXUTSDKMesh;
		
		V(mPointLightProxy->Create( d3dDevice, L".\\Media\\PointLightProxy.sdkmesh"));
		V(mSpotLightProxy->Create( d3dDevice, L".\\Media\\SpotLightProxy.sdkmesh"));
	}

	// Load noise texture
	D3DX11CreateShaderResourceViewFromFile( d3dDevice, L".\\Media\\Textures\\vector_noise.dds", NULL, NULL, &mNoiseSRV, NULL );
	DXUT_SetDebugName(mNoiseSRV, "mNoiseSRV");

	D3DX11CreateShaderResourceViewFromFile( d3dDevice, L".\\Media\\Textures\\BestFitNormal.dds", NULL, NULL, &mBestFitNormalSRV, NULL );
	DXUT_SetDebugName(mNoiseSRV, "mBestFitNormalSRV");
}

SSAO::~SSAO(void)
{
	SAFE_RELEASE(mMeshVertexLayout);
	SAFE_RELEASE(mLightProxyVertexLayout);
	
	SAFE_RELEASE(mDiffuseSampler);
	SAFE_RELEASE(mGBufferSampler);
	SAFE_RELEASE(mNoiseSampler);

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

	SAFE_RELEASE(mNoiseSRV);
	SAFE_RELEASE(mBestFitNormalSRV);
	SAFE_RELEASE(mDepthBufferReadOnlyDSV);
	
	SAFE_RELEASE(mGeometryBlendState);
	SAFE_RELEASE(mLightingBlendState);

	
	delete mPointLightProxy;	
	delete mSpotLightProxy;	
}

void SSAO::OnD3D11ResizedSwapChain( ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferDesc )
{
	mGBufferWidth = backBufferDesc->Width;
	mGBufferHeight = backBufferDesc->Height;

	SAFE_RELEASE(mDepthBufferReadOnlyDSV);

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
	mGBufferSRV.resize(mGBuffer.size() + 1);
	for (size_t i = 0; i < mGBuffer.size(); ++i)
	{
		mGBufferRTV[i] = mGBuffer[i]->GetRenderTargetView();
		mGBufferSRV[i] = mGBuffer[i]->GetShaderResourceView();
	}

	// Depth buffer is the last SRV that we use for reading
	mGBufferSRV.back() = mDepthBuffer->GetShaderResourceView();
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
		constants->NearFar = D3DXVECTOR4(viewerCamera.GetNearClip(), viewerCamera.GetFarClip(), 0.0f, 0.0f);

		d3dDeviceContext->Unmap(mPerFrameConstants, 0);
	}

	// Generate GBuffer
	RenderGBuffer(d3dDeviceContext, scene, viewerCamera, viewport);

	// Deferred shading
	ComputeShading(d3dDeviceContext, lights, viewerCamera, viewport);

	// Post-Process
	PostProcess(d3dDeviceContext, backBuffer, backDepth, viewport);
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
	d3dDeviceContext->PSSetSamplers(0, 1, &mGBufferSampler); // Point Sampler
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
	ID3D11RenderTargetView * renderTargets[1] = { mAOBuffer->GetRenderTargetView() };
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBuffer->GetDepthStencilView());

	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
	d3dDeviceContext->ClearRenderTargetView(renderTargets[0], zeros);

	// Fill AO constants
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mAOParamsConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		SSAOParams* constants = static_cast<SSAOParams *>(mappedResource.pData);

		constants->ViewParams = D3DXVECTOR4((float)mGBufferWidth, (float)mGBufferHeight, viewerCamera.GetNearClip(), viewerCamera.GetFarClip() );
		constants->AOParams = D3DXVECTOR4( 0.5f, 0.5f, 3.0f, 200.0f );	

		d3dDeviceContext->Unmap(mAOParamsConstants, 0);
	}

	
	// Render full sreen quad
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	// GBuffer normal and depth
	d3dDeviceContext->PSSetShaderResources(0, 1, &mGBufferSRV.front());
	d3dDeviceContext->PSSetShaderResources(1, 1, &mNoiseSRV);
	d3dDeviceContext->PSSetShader(mSSAOCrytekPS->GetShader(), 0, 0);
	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mAOParamsConstants);

    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

	d3dDeviceContext->Draw(3, 0);
}

void SSAO::ComputeShading( ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	std::shared_ptr<Texture2D> &accumulateBuffer = mLighting ? mLightAccumulateBuffer : mLitBuffer;

	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
	d3dDeviceContext->ClearRenderTargetView(accumulateBuffer->GetRenderTargetView(), zeros);

	// Set GBuffer, all light type needs it
	d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
	d3dDeviceContext->PSSetSamplers(0, 1, &mGBufferSampler);

	d3dDeviceContext->RSSetViewports(1, viewport);

	ID3D11RenderTargetView * renderTargets[1] = { accumulateBuffer->GetRenderTargetView() };
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBufferReadOnlyDSV);
	d3dDeviceContext->OMSetBlendState(mLightingBlendState, 0, 0xFFFFFFFF);

	DrawPointLight(d3dDeviceContext, lights, viewerCamera);
	//DrawDirectionalLight(d3dDeviceContext, lights, viewerCamera);

	if (mLighting)
	{
		// final shading pass
		const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
		d3dDeviceContext->ClearRenderTargetView(mLitBuffer->GetRenderTargetView(), zeros);

		d3dDeviceContext->IASetInputLayout(0);
		d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

		d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);	
		d3dDeviceContext->VSSetShader(mDeferredShadingVS[LT_DirectionalLigt]->GetShader(), 0, 0);

		ID3D11RenderTargetView * renderTargets[1] = { mLitBuffer->GetRenderTargetView() };
		d3dDeviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
		d3dDeviceContext->OMSetDepthStencilState(mDepthDisableState, 0);

		ID3D11ShaderResourceView* srv[3] = { mGBufferSRV[0],  mGBufferSRV[1], mLightAccumulateBuffer->GetShaderResourceView() };
		d3dDeviceContext->PSSetShaderResources(0, 3, srv);
		d3dDeviceContext->PSSetSamplers(0, 1, &mGBufferSampler);
		d3dDeviceContext->PSSetShader(mDeferredLightingShadingPS->GetShader(), 0, 0);

		d3dDeviceContext->RSSetState(mRasterizerState);
	
		d3dDeviceContext->Draw(3, 0);
	}

	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
	DrawLightVolumeDebug(d3dDeviceContext, lights, viewerCamera);

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

	d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);	
	d3dDeviceContext->VSSetShader(mDeferredShadingVS[LT_DirectionalLigt]->GetShader(), 0, 0);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetConstantBuffers(1, 1, &mLightConstants);
	d3dDeviceContext->PSSetShader(
		mLighting ? mDeferredLightingPS[LT_DirectionalLigt]->GetShader() : mDeferredShadingPS[LT_DirectionalLigt]->GetShader(), 0, 0);

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

		const D3DXVECTOR3& lightDiection = lights.mLights[idx].LightDirection;

		D3DXVec3TransformNormal(&lightCBffer->LightDirection, &lightDiection, viewerCamera.GetViewMatrix());
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

	bool useScreenQuad = true;

	if (useScreenQuad)
	{
		d3dDeviceContext->IASetInputLayout(0);
		d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);
		d3dDeviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

		d3dDeviceContext->VSSetShader(mScreenQuadVS->GetShader(), 0, 0);
		d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);	
		d3dDeviceContext->VSSetConstantBuffers(1, 1, &mLightConstants);	

		d3dDeviceContext->GSSetShader(mScreenQuadGS->GetShader(), 0, 0);

		d3dDeviceContext->PSSetShader(mLighting ? mDeferredLightingPS[LT_PointLight]->GetShader() : mDeferredShadingPS[LT_PointLight]->GetShader(), 0, 0);
		d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
		d3dDeviceContext->PSSetConstantBuffers(1, 1, &mLightConstants);
	}
	else
	{
		d3dDeviceContext->IASetInputLayout(mLightProxyVertexLayout);
		d3dDeviceContext->VSSetShader(mDeferredShadingVS[LT_PointLight]->GetShader(), 0, 0);
		d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerObjectConstants);	

		d3dDeviceContext->PSSetShader(mLighting ? mDeferredLightingPS[LT_PointLight]->GetShader() : mDeferredShadingPS[LT_PointLight]->GetShader(), 0, 0);
		d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
		d3dDeviceContext->PSSetConstantBuffers(1, 1, &mLightConstants);		
	}

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

		// Fill per frame constants
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

			D3DXVECTOR4 bound = CalculateLightBound(lightCBuffer->LightPosition, lightRadius, viewerCamera.GetNearClip(), cameraProj(0,0), cameraProj(1,1));

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

		break;
	}	

	d3dDeviceContext->GSSetShader(0, 0, 0);
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

		float lightRadius = light.LightAttenuation.y / 10.0f;  // Attenuation End
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

		break;
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
	d3dDeviceContext->PSSetSamplers(0, 1, &mGBufferSampler);
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
	ID3D11ShaderResourceView* srv[] = { mLitBuffer->GetShaderResourceView() };
	d3dDeviceContext->PSSetShaderResources(0, 1, srv);
	d3dDeviceContext->PSSetSamplers(0, 1, &mGBufferSampler);

	d3dDeviceContext->PSSetShader(mFullQuadSprite->GetShader(), 0, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
	d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, backDepth);

	d3dDeviceContext->Draw(3, 0);
}


