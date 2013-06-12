#include "DXUT.h"
#include "SSAO.h"
#include "Texture2D.h"
#include "Shader.h"
#include "DXUTcamera.h"
#include "SDKMesh.h"
#include "LightAnimation.h"
#include "Scene.h"

// NOTE: Must match layout of shader constant buffers

__declspec(align(16))
struct PerFrameConstants
{
	D3DXMATRIX WorldViewProj;
	D3DXMATRIX WorldView;
	D3DXMATRIX Proj;
	D3DXMATRIX InvProj;
	D3DXVECTOR4 NearFar;
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
	Float4Align D3DXVECTOR3 LightPosVS;        // View space light position
	Float4Align D3DXVECTOR3 LightDirectionVS; 
	Float4Align D3DXVECTOR2 LightFalloff;   // begin and end
};
#pragma warning( pop ) 

namespace {

D3DXVECTOR3 HueToRGB(float hue)
{
	float intPart;
	float fracPart = modf(hue * 6.0f, &intPart);
	int region = static_cast<int>(intPart);

	switch (region) {
	case 0: return D3DXVECTOR3(1.0f, fracPart, 0.0f);
	case 1: return D3DXVECTOR3(1.0f - fracPart, 1.0f, 0.0f);
	case 2: return D3DXVECTOR3(0.0f, 1.0f, fracPart);
	case 3: return D3DXVECTOR3(0.0f, 1.0f - fracPart, 1.0f);
	case 4: return D3DXVECTOR3(fracPart, 0.0f, 1.0f);
	case 5: return D3DXVECTOR3(1.0f, 0.0f, 1.0f - fracPart);
	};

	return D3DXVECTOR3(0.0f, 0.0f, 0.0f);
}

}

SSAO::SSAO( ID3D11Device* d3dDevice )
	: mDepthBufferReadOnlyDSV(0)
{
	mFullQuadSprite = std::make_shared<PixelShader>(d3dDevice, L".\\Media\\Shaders\\FullQuadSprite.hlsl", "FullQuadSpritePS", nullptr);

	mGeometryVS = std::make_shared<VertexShader>(d3dDevice, L".\\Media\\Shaders\\Rendering.hlsl", "GeometryVS", nullptr);
	mGeometryPS = std::make_shared<PixelShader>(d3dDevice, L".\\Media\\Shaders\\Rendering.hlsl", "GeometryPS", nullptr);

	mFullScreenTriangleVS = std::make_shared<VertexShader>(d3dDevice, L".\\Media\\Shaders\\FullScreenTriangle.hlsl", "FullScreenTriangleVS", nullptr);
	mSSAOCrytekPS = std::make_shared<PixelShader>(d3dDevice, L".\\Media\\Shaders\\SSAO.hlsl", "SSAOPS", nullptr);

	{
		mForwardDirectionalVS = std::make_shared<VertexShader>(d3dDevice, L".\\Media\\Shaders\\ForwardRendering.hlsl", "ForwardVS", nullptr);
		mForwardDirectionalPS = std::make_shared<PixelShader>(d3dDevice, L".\\Media\\Shaders\\ForwardRendering.hlsl", "ForwardPS", nullptr);
	}

	{
		D3D10_SHADER_MACRO defines[] = {
			{"PointLight", ""},
			{"SpotLight", ""},
			{0, 0}
		};
		mDeferredPointOrSpotVS = std::make_shared<VertexShader>(d3dDevice, L".\\Media\\Shaders\\DeferredRendering.hlsl", "DeferredRenderingVS", defines);
		mDeferredPointPS = std::make_shared<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredRendering.hlsl", "DeferredRenderingPointPS", nullptr);
	}
	
	{
		D3D10_SHADER_MACRO defines[] = {
			{"DirectionalLight", ""},
			{0, 0}
		};
		mDeferredDirectionalVS = std::make_shared<VertexShader>(d3dDevice, L".\\Media\\Shaders\\DeferredRendering.hlsl", "DeferredRenderingVS", defines);
		mDeferredDirectionalPS = std::make_shared<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredRendering.hlsl", "DeferredRenderingDirectionPS", nullptr);
	}

	// Create mesh vertex layout
	{	
		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		mMeshVertexLayout = CreateVertexLayout(d3dDevice, layout, ARRAYSIZE(layout),  L".\\Media\\Shaders\\Rendering.hlsl", "GeometryVS", NULL);
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
		mLightProxyVertexLayout = CreateVertexLayout(d3dDevice, layout, ARRAYSIZE(layout),  L".\\Media\\Shaders\\DeferredRendering.hlsl", "DeferredRenderingVS", defines);
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

		if (!mPointLightProxy->IsLoaded())
		{
			throw std::exception("Error");
		}
	}

	// Load noise texture
	D3DX11CreateShaderResourceViewFromFile( d3dDevice, L".\\Media\\Textures\\vector_noise.dds", NULL, NULL, &mNoiseSRV, NULL );
	DXUT_SetDebugName(mNoiseSRV, "mNoiseSRV");

	mDebugVS = std::make_shared<VertexShader>(d3dDevice, L".\\Media\\Shaders\\DeferredRendering.hlsl", "DebugPointLightVS", nullptr);
	mDebugPS = std::make_shared<PixelShader>(d3dDevice, L".\\Media\\Shaders\\DeferredRendering.hlsl", "DebugPointLightPS", nullptr);
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

	SAFE_RELEASE(mPerFrameConstants);
	SAFE_RELEASE(mAOParamsConstants);
	SAFE_RELEASE(mLightConstants);

	SAFE_RELEASE(mNoiseSRV);
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
		DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

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
		DXGI_FORMAT_R8G8B8A8_UNORM/*DXGI_FORMAT_R16G16B16A16_FLOAT*/, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET));

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
	d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
	
	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	d3dDeviceContext->PSSetShader(mForwardDirectionalPS->GetShader(), 0, 0);
	d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
	//d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetConstantBuffers(1, 1, &mLightConstants);

	d3dDeviceContext->OMSetDepthStencilState(mDepthState, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
	d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, backDepth);

	const D3DXMATRIXA16& cameraProj = *viewerCamera.GetProjMatrix();
	const D3DXMATRIXA16& cameraView = *viewerCamera.GetViewMatrix();
	
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

		D3DXVec3TransformNormal(&lightCBffer->LightDirectionVS, &lightDiection, viewerCamera.GetViewMatrix());
		lightCBffer->LightColor = lights.mLights[idx].LightColor;
		d3dDeviceContext->Unmap(mLightConstants, 0);

		for (size_t i = 0; i < scene.mSceneMeshesOpaque.size(); ++i)
		{
			// Compute composite matrices
			const D3DXMATRIXA16 WorldView = scene.mSceneMeshesOpaque[i].World * cameraView;
			const D3DXMATRIXA16 WorldViewProj = WorldView * cameraProj;

			// Fill per frame constants
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			PerFrameConstants* constants = static_cast<PerFrameConstants *>(mappedResource.pData);

			constants->WorldView = WorldView;
			constants->WorldViewProj = WorldViewProj;

			d3dDeviceContext->Unmap(mPerFrameConstants, 0);

			scene.mSceneMeshesOpaque[i].Mesh->Render(d3dDeviceContext, 0);
		}

		idx++;
	}	
}

void SSAO::Render( ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
	const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	// Generate GBuffer
	RenderGBuffer(d3dDeviceContext, scene, viewerCamera, viewport);

	// Deferred shading
	ComputeShading(d3dDeviceContext, lights, viewerCamera, viewport);

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

void SSAO::RenderGBuffer( ID3D11DeviceContext* d3dDeviceContext, const Scene& scene, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	// Clear GBuffer
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	for (size_t i = 0; i < mGBufferRTV.size(); ++i)
		d3dDeviceContext->ClearRenderTargetView(mGBufferRTV[i], zeros);

	// Clear Depth 
	d3dDeviceContext->ClearDepthStencilView(mDepthBuffer->GetDepthStencilView(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

	d3dDeviceContext->VSSetShader(mGeometryVS->GetShader(), 0, 0);
	d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
	d3dDeviceContext->PSSetShader(mGeometryPS->GetShader(), 0, 0);

	d3dDeviceContext->OMSetDepthStencilState(mDepthState, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
	d3dDeviceContext->OMSetRenderTargets(mGBufferRTV.size(), &mGBufferRTV[0], mDepthBuffer->GetDepthStencilView());

	const D3DXMATRIXA16& cameraProj = *viewerCamera.GetProjMatrix();
	const D3DXMATRIXA16& cameraView = *viewerCamera.GetViewMatrix();
	const D3DXVECTOR4 cameraNearFar(viewerCamera.GetNearClip(), viewerCamera.GetFarClip(), 0.0f, 0.0f);
	
	for (size_t i = 0; i < scene.mSceneMeshesOpaque.size(); ++i)
	{
		// Compute composite matrices
		const D3DXMATRIXA16 WorldView = scene.mSceneMeshesOpaque[i].World * cameraView;
		const D3DXMATRIXA16 WorldViewProj = WorldView * cameraProj;

		// Fill per frame constants
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			PerFrameConstants* constants = static_cast<PerFrameConstants *>(mappedResource.pData);

			constants->WorldView = WorldView;
			constants->WorldViewProj = WorldViewProj;
			constants->NearFar = cameraNearFar;

			d3dDeviceContext->Unmap(mPerFrameConstants, 0);
		}

		scene.mSceneMeshesOpaque[i].Mesh->Render(d3dDeviceContext, 0);
	}
	
	// Cleanup (aka make the runtime happy)
	d3dDeviceContext->VSSetShader(0, 0, 0);
	d3dDeviceContext->PSSetShader(0, 0, 0);
	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
	ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
	d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
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
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
	d3dDeviceContext->ClearRenderTargetView(mLitBuffer->GetRenderTargetView(), zeros);

	// Set GBuffer, all light type needs it
	d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
	d3dDeviceContext->PSSetSamplers(0, 1, &mGBufferSampler);

	d3dDeviceContext->RSSetViewports(1, viewport);

	ID3D11RenderTargetView * renderTargets[1] = { mLitBuffer->GetRenderTargetView() };
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBufferReadOnlyDSV);
	d3dDeviceContext->OMSetBlendState(mLightingBlendState, 0, 0xFFFFFFFF);

	DrawPointLight(d3dDeviceContext, lights, viewerCamera);
	DrawDirectionalLight(d3dDeviceContext, lights, viewerCamera);

	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
	DrawLightVolumeDebug(d3dDeviceContext, lights, viewerCamera);

	// Cleanup (aka make the runtime happy)
	d3dDeviceContext->VSSetShader(0, 0, 0);
	d3dDeviceContext->GSSetShader(0, 0, 0);
	d3dDeviceContext->PSSetShader(0, 0, 0);
	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
	ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
	d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
}

void SSAO::DrawDirectionalLight( ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera )
{
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);	
	d3dDeviceContext->VSSetShader(mDeferredDirectionalVS->GetShader(), 0, 0);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetConstantBuffers(1, 1, &mLightConstants);
	d3dDeviceContext->PSSetShader(mDeferredDirectionalPS->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->OMSetDepthStencilState(mDepthDisableState, 0);

	// Fill per frame constants
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		PerFrameConstants* constants = static_cast<PerFrameConstants*>(mappedResource.pData);

		// Only need those 
		constants->NearFar = D3DXVECTOR4(viewerCamera.GetNearClip(), viewerCamera.GetFarClip(), 0.0f, 0.0f);
		D3DXMatrixInverse(&constants->InvProj, NULL, viewerCamera.GetProjMatrix());

		d3dDeviceContext->Unmap(mPerFrameConstants, 0);
	}

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

		D3DXVec3TransformNormal(&lightCBffer->LightDirectionVS, &lightDiection, viewerCamera.GetViewMatrix());
		lightCBffer->LightColor = lights.mLights[idx].LightColor;
		d3dDeviceContext->Unmap(mLightConstants, 0);

		d3dDeviceContext->Draw(3, 0);

		idx++;
	}	
}

void SSAO::DrawPointLight( ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera )
{
	const D3DXMATRIXA16& cameraProj = *viewerCamera.GetProjMatrix();
	const D3DXMATRIXA16& cameraView = *viewerCamera.GetViewMatrix();

	d3dDeviceContext->IASetInputLayout(mLightProxyVertexLayout);

	d3dDeviceContext->VSSetShader(mDeferredPointOrSpotVS->GetShader(), 0, 0);
	d3dDeviceContext->PSSetShader(mDeferredPointPS->GetShader(), 0, 0);

	size_t idx = 0;
	while(idx < lights.mLights.size() && lights.mLights[idx].LightType != LT_PointLight) idx++;

	while(idx < lights.mLights.size() && lights.mLights[idx].LightType == LT_PointLight)
	{
		const LightAnimation::Light& light = lights.mLights[idx];

		float lightRadius = light.LightFalloff.y;  // Attenuation End
		const D3DXVECTOR3& lightPosition = light.LightPosition;

		D3DXMATRIXA16 world;
		D3DXMatrixScaling(&world, lightRadius, lightRadius, lightRadius);   // Scale
		world._41 = lightPosition.x;
		world._42 = lightPosition.y;
		world._43 = lightPosition.z;         // translation

		// Compute composite matrices
		const D3DXMATRIXA16 WorldView = world * cameraView;
		const D3DXMATRIXA16 WorldViewProj = WorldView * cameraProj;

		// Fill per frame constants
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			PerFrameConstants* constants = static_cast<PerFrameConstants *>(mappedResource.pData);

			// Only need those two
			constants->WorldView = WorldView;
			constants->WorldViewProj = WorldViewProj;
			constants->NearFar = D3DXVECTOR4(viewerCamera.GetNearClip(), viewerCamera.GetFarClip(), 0.0f, 0.0f);

			d3dDeviceContext->Unmap(mPerFrameConstants, 0);
		}

		// Fill PointLight constants
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mLightConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			LightCBuffer* lightCBuffer = static_cast<LightCBuffer*>(mappedResource.pData);

			D3DXVec3TransformCoord(&lightCBuffer->LightPosVS, &lightPosition, &cameraView);
			lightCBuffer->LightColor = light.LightColor;
			lightCBuffer->LightFalloff = light.LightFalloff;

			d3dDeviceContext->Unmap(mLightConstants, 0);
		}

		d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);	

		d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
		d3dDeviceContext->PSSetConstantBuffers(1, 1, &mLightConstants);

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

		idx++;
	}	
}

void SSAO::DrawLightVolumeDebug( ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera )
{
	const D3DXMATRIXA16& cameraProj = *viewerCamera.GetProjMatrix();
	const D3DXMATRIXA16& cameraView = *viewerCamera.GetViewMatrix();

	d3dDeviceContext->IASetInputLayout(mLightProxyVertexLayout);

	d3dDeviceContext->VSSetShader(mDebugVS->GetShader(), 0, 0);
	d3dDeviceContext->PSSetShader(mDebugPS->GetShader(), 0, 0);

	size_t idx = 0;
	while(idx < lights.mLights.size() && lights.mLights[idx].LightType != LT_PointLight) idx++;

	while(idx < lights.mLights.size() && lights.mLights[idx].LightType == LT_PointLight)
	{
		const LightAnimation::Light& light = lights.mLights[idx];

		float lightRadius = light.LightFalloff.y / 10.0f;  // Attenuation End
		const D3DXVECTOR3& lightPosition = light.LightPosition;

		D3DXMATRIXA16 world;
		D3DXMatrixScaling(&world, lightRadius, lightRadius, lightRadius);   // Scale
		world._41 = lightPosition.x;
		world._42 = lightPosition.y;
		world._43 = lightPosition.z;         // translation

		// Compute composite matrices
		const D3DXMATRIXA16 WorldView = world * cameraView;
		const D3DXMATRIXA16 WorldViewProj = WorldView * cameraProj;

		// Fill per frame constants
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			PerFrameConstants* constants = static_cast<PerFrameConstants *>(mappedResource.pData);

			// Only need those two
			constants->WorldView = WorldView;
			constants->WorldViewProj = WorldViewProj;
			constants->NearFar = D3DXVECTOR4(viewerCamera.GetNearClip(), viewerCamera.GetFarClip(), 0.0f, 0.0f);

			d3dDeviceContext->Unmap(mPerFrameConstants, 0);
		}

		// Fill PointLight constants
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			d3dDeviceContext->Map(mLightConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			LightCBuffer* lightCBuffer = static_cast<LightCBuffer*>(mappedResource.pData);

			D3DXVec3TransformCoord(&lightCBuffer->LightPosVS, &lightPosition, &cameraView);
			lightCBuffer->LightColor = light.LightColor;
			lightCBuffer->LightFalloff = light.LightFalloff;

			d3dDeviceContext->Unmap(mLightConstants, 0);
		}

		d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);	

		d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
		d3dDeviceContext->PSSetConstantBuffers(1, 1, &mLightConstants);

		d3dDeviceContext->OMSetDepthStencilState(mDepthLEQualState, 0);
		d3dDeviceContext->RSSetState(mRasterizerState);

		mPointLightProxy->Render(d3dDeviceContext);

		idx++;
	}	
}

