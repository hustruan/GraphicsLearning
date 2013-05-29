#include "DXUT.h"
#include "SSAO.h"
#include "Texture2D.h"
#include "Shader.h"
#include "DXUTcamera.h"
#include "SDKMesh.h"


// NOTE: Must match layout of shader constant buffers

__declspec(align(16))
struct PerFrameConstants
{
	D3DXMATRIX WorldViewProj;
	D3DXMATRIX WorldView;
	D3DXMATRIX ViewProj;
	D3DXMATRIX Proj;
	D3DXVECTOR4 NearFar;
};

__declspec(align(16))
struct SSAOParams
{
	D3DXVECTOR4 ViewParams;
	D3DXVECTOR4 AOParams;
};


namespace {

void ComputeFrustumExtents(const D3DXMATRIXA16& matViewProj, D3DXVECTOR3* pOut)
{
	const int CornerCount = 8;
	const int PlaneCount = 6;

	D3DXPLANE planes[PlaneCount];
	planes[0] = D3DXPLANE(-matViewProj._13, -matViewProj._23, -matViewProj._33, -matViewProj._43);
	planes[1] = D3DXPLANE(matViewProj._13 - matViewProj._14, matViewProj._23 - matViewProj._24, matViewProj._33 - matViewProj._34, matViewProj._43 - matViewProj._44);
	planes[2] = D3DXPLANE(-matViewProj._14 - matViewProj._11, -matViewProj._24 - matViewProj._21, -matViewProj._34 - matViewProj._31, -matViewProj._44 - matViewProj._41);
	planes[3] = D3DXPLANE(matViewProj._11 - matViewProj._14, matViewProj._21 - matViewProj._24, matViewProj._31 - matViewProj._34, matViewProj._41 - matViewProj._44);
	planes[4] = D3DXPLANE(matViewProj._12 - matViewProj._14, matViewProj._22 - matViewProj._24, matViewProj._32 - matViewProj._34, matViewProj._42 - matViewProj._44);
	planes[5] = D3DXPLANE(-matViewProj._14 - matViewProj._12, -matViewProj._24 - matViewProj._22, -matViewProj._34 - matViewProj._32, -matViewProj._44 - matViewProj._42);

	for(int i = 0; i < PlaneCount; ++i)
		D3DXPlaneNormalize(&planes[i], &planes[i]);

	D3DXVECTOR3 corners[CornerCount];
	IntersectionPoint(ref this.planes[0], ref this.planes[2], ref this.planes[4], out this.corners[0]);
	IntersectionPoint(ref this.planes[0], ref this.planes[3], ref this.planes[4], out this.corners[1]);
	IntersectionPoint(ref this.planes[0], ref this.planes[3], ref this.planes[5], out this.corners[2]);
	IntersectionPoint(ref this.planes[0], ref this.planes[2], ref this.planes[5], out this.corners[3]);
	IntersectionPoint(ref this.planes[1], ref this.planes[2], ref this.planes[4], out this.corners[4]);
	IntersectionPoint(ref this.planes[1], ref this.planes[3], ref this.planes[4], out this.corners[5]);
	IntersectionPoint(ref this.planes[1], ref this.planes[3], ref this.planes[5], out this.corners[6]);
	IntersectionPoint(ref this.planes[1], ref this.planes[2], ref this.planes[5], out this.corners[7]);
}



}



SSAO::SSAO( ID3D11Device* d3dDevice )
{
	mGeometryVS = std::make_shared<VertexShader>(d3dDevice, L"./Media/Shaders/Rendering.hlsl", "GeometryVS", nullptr);
	mGeometryPS = std::make_shared<PixelShader>(d3dDevice, L"./Media/Shaders/Rendering.hlsl", "GeometryPS", nullptr);

	mFullScreenTriangleVS = std::make_shared<VertexShader>(d3dDevice, L"./Media/Shaders/FullScreenTriangle.hlsl", "FullScreenTriangleVS", nullptr);
	mSSAOCrytekPS = std::make_shared<PixelShader>(d3dDevice, L"./Media/Shaders/SSAO.hlsl", "SSAOPS", nullptr);

	// Load noise texture
	D3DX11CreateShaderResourceViewFromFile( d3dDevice, L"./Media/Textures/vector_noise.dds", NULL, NULL, &mNoiseSRV, NULL );

	HRESULT hr;

	// Create mesh vertex layout
	{
		UINT shaderFlags = D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;
		ID3D10Blob *bytecode = 0;
		hr = D3DX11CompileFromFile(L"./Media/Shaders/Rendering.hlsl", NULL, NULL, "GeometryVS", "vs_4_0", 
			shaderFlags, 0, 0, &bytecode, 0, 0);

		assert(SUCCEEDED(hr));

		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		hr = d3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), bytecode->GetBufferPointer(), 
			bytecode->GetBufferSize(), &mMeshVertexLayout);
		assert(SUCCEEDED(hr));

		bytecode->Release();

		//ID3D11ShaderReflection* pReflector = NULL;
		//hr = D3DReflect( bytecode->GetBufferPointer(),  bytecode->GetBufferSize(), IID_ID3D11ShaderReflection, 
		//	(void**) &pReflector);

		//D3D11_SHADER_DESC desc;
		//pReflector->GetDesc( &desc );

		//for ( UINT i = 0; i < desc.OutputParameters; i++ )
		//{
		//	D3D11_SIGNATURE_PARAMETER_DESC output_desc;
		//	pReflector->GetOutputParameterDesc( i, &output_desc );
		//	// Do something with the description¡­

		//	int a = 0;
		//}

		//struct ConstantBufferLayout
		//{
		//	D3D11_SHADER_BUFFER_DESC Description;
		//	std::vector<D3D11_SHADER_VARIABLE_DESC> Variables;
		//	std::vector<D3D11_SHADER_TYPE_DESC> Types;
		//};

		//for ( UINT i = 0; i < desc.ConstantBuffers; i++ )
		//{
		//	ConstantBufferLayout BufferLayout;
		//	ID3D11ShaderReflectionConstantBuffer* pConstBuffer = pReflector->GetConstantBufferByIndex( i );
		//	pConstBuffer->GetDesc( &BufferLayout.Description );
		//	// Load the description of each variable for use later on when binding a buffer
		//		for ( UINT j = 0; j < BufferLayout.Description.Variables; j++ )
		//		{
		//			// Get the variable description and store it
		//			ID3D11ShaderReflectionVariable* pVariable = pConstBuffer->GetVariableByIndex( j );
		//			D3D11_SHADER_VARIABLE_DESC var_desc;
		//			pVariable->GetDesc( &var_desc );
		//			BufferLayout.Variables.push_back( var_desc );
		//			// Get the variable type description and store it
		//			ID3D11ShaderReflectionType* pType = pVariable->GetType();
		//			D3D11_SHADER_TYPE_DESC type_desc;
		//			pType->GetDesc( &type_desc );
		//			BufferLayout.Types.push_back( type_desc );
		//		}
		//}
	}

	// Create standard rasterizer state
	{
		CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
		d3dDevice->CreateRasterizerState(&desc, &mRasterizerState);
	}

	// Create standard depth state
	{
		CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
		d3dDevice->CreateDepthStencilState(&desc, &mDepthState);
	}

	// Create geometry phase blend state
	{
		CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
		d3dDevice->CreateBlendState(&desc, &mGeometryBlendState);
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
	}

	// Create sampler for random vector
	{
		CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		d3dDevice->CreateSamplerState(&desc, &mNoiseSampler);
	}

	// Create sampler for GBuffer
	{
		CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
		desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		d3dDevice->CreateSamplerState(&desc, &mGBufferSampler);	
	}

	// Create constant buffer
	{
		CD3D11_BUFFER_DESC desc(sizeof(PerFrameConstants), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		d3dDevice->CreateBuffer(&desc, nullptr, &mPerFrameConstants);
	}

	{
		CD3D11_BUFFER_DESC desc(sizeof(SSAOParams), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		d3dDevice->CreateBuffer(&desc, nullptr, &mAOParamsConstants);
	}

	
}


SSAO::~SSAO(void)
{
	SAFE_RELEASE(mMeshVertexLayout);
	SAFE_RELEASE(mGeometryBlendState);
	SAFE_RELEASE(mDiffuseSampler);
	SAFE_RELEASE(mRasterizerState);
	SAFE_RELEASE(mDepthState);
	SAFE_RELEASE(mPerFrameConstants);
	SAFE_RELEASE(mGBufferSampler);
	SAFE_RELEASE(mNoiseSampler);
	SAFE_RELEASE(mNoiseSRV);
	SAFE_RELEASE(mAOParamsConstants);
	SAFE_RELEASE(mDepthBufferReadOnlyDSV);
	
}

void SSAO::OnD3D11ResizedSwapChain( ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferDesc )
{
	mGBufferWidth = backBufferDesc->Width;
	mGBufferHeight = backBufferDesc->Height;

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
		DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET));

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

void SSAO::Render( ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth, CDXUTSDKMesh& sceneMesh, const D3DXMATRIX& worldMatrix, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	const D3DXMATRIXA16& cameraProj = *viewerCamera.GetProjMatrix();
	const D3DXMATRIXA16& cameraView = *viewerCamera.GetViewMatrix();

	// Compute composite matrices
	D3DXMATRIXA16 cameraViewProj = cameraView * cameraProj;
	D3DXMATRIXA16 cameraWorldViewProj = worldMatrix * cameraViewProj;

	// Fill per frame constants
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		PerFrameConstants* constants = static_cast<PerFrameConstants *>(mappedResource.pData);

		constants->WorldViewProj = cameraWorldViewProj;
		constants->ViewProj = cameraViewProj;
		constants->Proj = cameraProj;
		constants->WorldView = worldMatrix * cameraView;
		constants->NearFar = D3DXVECTOR4(viewerCamera.GetNearClip(), viewerCamera.GetFarClip(), 0.0f, 0.0f);
	
		d3dDeviceContext->Unmap(mPerFrameConstants, 0);
	}
	
	// Fill AO constants
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mAOParamsConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		SSAOParams* constants = static_cast<SSAOParams *>(mappedResource.pData);

		constants->ViewParams = D3DXVECTOR4((float)mGBufferWidth, (float)mGBufferHeight, viewerCamera.GetNearClip(), viewerCamera.GetFarClip() );
		constants->AOParams = D3DXVECTOR4( 0.5f, 0.5f, 3.0f, 200.0f );	

		d3dDeviceContext->Unmap(mAOParamsConstants, 0);
	}


	RenderGBuffer(d3dDeviceContext, sceneMesh, viewerCamera, viewport);

	// Todo 
	d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, backDepth);

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
	d3dDeviceContext->PSSetSamplers(0, 1, &mGBufferSampler);
	d3dDeviceContext->PSSetSamplers(1, 1, &mNoiseSampler);
	d3dDeviceContext->PSSetShader(mSSAOCrytekPS->GetShader(), 0, 0);
	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mAOParamsConstants);

	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

	d3dDeviceContext->Draw(3, 0);
}


void SSAO::RenderGBuffer( ID3D11DeviceContext* d3dDeviceContext, CDXUTSDKMesh& sceneMesh, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	d3dDeviceContext->OMSetRenderTargets(mGBufferRTV.size(), &mGBufferRTV[0], mDepthBuffer->GetDepthStencilView());

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
	
	sceneMesh.Render(d3dDeviceContext, 0);
}

void SSAO::RenderSSAO( ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport )
{
	ID3D11RenderTargetView * renderTargets[1] = { mAOBuffer->GetRenderTargetView() };
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBuffer->GetDepthStencilView());

	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
	d3dDeviceContext->ClearRenderTargetView(renderTargets[0], zeros);
	
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

void SSAO::ComputeShading( ID3D11DeviceContext* d3dDeviceContext, const D3D11_VIEWPORT* viewport )
{
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};	
	d3dDeviceContext->ClearRenderTargetView(mLitBuffer->GetRenderTargetView(), zeros);

	// Full screen triangle setup
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetShaderResources(0, mGBufferSRV.size(), &mGBufferSRV.front());
	//d3dDeviceContext->PSSetShader()


	ID3D11RenderTargetView * renderTargets[1] = { mLitBuffer->GetRenderTargetView() };
	d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBufferReadOnlyDSV);
	d3dDeviceContext->OMSetDepthStencilState(mDepthState, 0);
	
	d3dDeviceContext->Draw(3, 0);
}
