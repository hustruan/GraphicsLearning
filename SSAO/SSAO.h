#pragma once

#include "DXUT.h"
#include "SDKmisc.h"
#include "Shader.h"
#include <vector>
#include <memory>
#include <algorithm>

#define Float4Align __declspec(align(16))

using std::shared_ptr;

template<typename T> class Shader;

class CDXUTSDKMesh;
class CFirstPersonCamera;
class Texture2D;
class Scene;
class LightAnimation;

class SSAO
{
public:

public:
	SSAO(ID3D11Device* d3dDevice);
	~SSAO(void);

	void OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferDesc);

	void Render(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
		const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	/**
	 * Forward only render the directional light
	 */
	void RenderForward(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
		const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

private:

	void RenderGBuffer(ID3D11DeviceContext* d3dDeviceContext, const Scene& scene, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void ComputeShading(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void RenderSSAO(ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void DrawPointLight(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void DrawSpotLight(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void DrawDirectionalLight(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void DrawLightVolumeDebug(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

private:

	UINT mGBufferWidth, mGBufferHeight; 

	ID3D11InputLayout* mMeshVertexLayout;
	ID3D11InputLayout* mLightProxyVertexLayout;

	ID3D11Buffer* mPerFrameConstants;
	ID3D11Buffer* mAOParamsConstants;
	ID3D11Buffer* mLightConstants;

	shared_ptr<Texture2D> mDepthBuffer;
	ID3D11DepthStencilView* mDepthBufferReadOnlyDSV;

	// GBuffer Shaders
	shared_ptr<VertexShader> mGeometryVS;
	shared_ptr<PixelShader> mGeometryPS;

	std::vector<shared_ptr<Texture2D>> mGBuffer;
	std::vector<ID3D11RenderTargetView*> mGBufferRTV;
	std::vector<ID3D11ShaderResourceView*> mGBufferSRV;

	// Deferred Shading Lit Buffer
	shared_ptr<Texture2D> mLitBuffer;
	shared_ptr<Texture2D> mAOBuffer;

	shared_ptr<VertexShader> mForwardDirectionalVS;
	shared_ptr<PixelShader> mForwardDirectionalPS;

	shared_ptr<VertexShader> mDeferredDirectionalVS;
	shared_ptr<PixelShader> mDeferredDirectionalPS;

	shared_ptr<VertexShader> mDeferredPointOrSpotVS;
	shared_ptr<PixelShader> mDeferredPointPS;
	shared_ptr<PixelShader> mDeferredSpotPS;

	shared_ptr<VertexShader> mDebugVS;;
	shared_ptr<PixelShader> mDebugPS;

	// SSAO Shaders
	shared_ptr<VertexShader> mFullScreenTriangleVS;
	shared_ptr<PixelShader> mSSAOCrytekPS;

	// Noise Texture
	ID3D11ShaderResourceView* mNoiseSRV;
	ID3D11ShaderResourceView* mBestFitNormalSRV;
	
	ID3D11SamplerState* mDiffuseSampler;
	ID3D11SamplerState* mNoiseSampler;
	ID3D11SamplerState* mGBufferSampler;
	
	ID3D11RasterizerState* mRasterizerState;       // Cull back
	ID3D11RasterizerState* mRasterizerFrontState;  // Cull front

	ID3D11DepthStencilState* mDepthState;
	ID3D11DepthStencilState* mDepthGreaterState;
	ID3D11DepthStencilState* mDepthLEQualState;
	ID3D11DepthStencilState* mDepthDisableState;   // Depth Disable

	ID3D11BlendState* mGeometryBlendState;
	ID3D11BlendState* mLightingBlendState;

	CDXUTSDKMesh* mPointLightProxy;
	CDXUTSDKMesh* mSpotLightProxy;

	shared_ptr<PixelShader> mFullQuadSprite;
};

