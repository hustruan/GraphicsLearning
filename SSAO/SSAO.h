#pragma once

#include "DXUT.h"
#include "SDKmisc.h"
#include "Shader.h"
#include <vector>
#include <memory>
#include <algorithm>

using std::shared_ptr;

class CDXUTSDKMesh;
class CFirstPersonCamera;
class Texture2D;


template<typename T>
class Shader;

class SSAO
{
public:

public:
	SSAO(ID3D11Device* d3dDevice);
	~SSAO(void);

	void OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferDesc);

	void Render(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
		CDXUTSDKMesh& sceneMesh, const D3DXMATRIX& worldMatrix, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport );

private:
	void RenderGBuffer(ID3D11DeviceContext* d3dDeviceContext, CDXUTSDKMesh& sceneMesh, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void ComputerLighting(ID3D11DeviceContext* d3dDeviceContext, const D3D11_VIEWPORT* viewport);

	void RenderSSAO(ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);
	
private:

	UINT mGBufferWidth, mGBufferHeight; 

	ID3D11InputLayout* mMeshVertexLayout;

	ID3D11Buffer* mPerFrameConstants;
	ID3D11Buffer* mAOParamsConstants;

	shared_ptr<Texture2D> mDepthBuffer;

	// GBuffer Shaders
	shared_ptr<VertexShader> mGeometryVS;
	shared_ptr<PixelShader> mGeometryPS;

	std::vector<shared_ptr<Texture2D>> mGBuffer;
	std::vector<ID3D11RenderTargetView*> mGBufferRTV;
	std::vector<ID3D11ShaderResourceView*> mGBufferSRV;

	// SSAO Shaders
	shared_ptr<VertexShader> mFullScreenTriangleVS;
	shared_ptr<PixelShader> mSSAOCrytekPS;

	shared_ptr<Texture2D> mAOBuffer;

	// Noise Texture
	ID3D11ShaderResourceView* mNoiseSRV;
	
	
	ID3D11SamplerState* mDiffuseSampler;
	ID3D11SamplerState* mNoiseSampler;
	ID3D11SamplerState* mGBufferSampler;
	
	ID3D11RasterizerState* mRasterizerState;
	ID3D11DepthStencilState* mDepthState;
	ID3D11BlendState* mGeometryBlendState;


};

