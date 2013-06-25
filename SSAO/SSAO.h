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

enum LightCullTechnique
{
	Cull_Forward_None = 0,
	Cull_Forward_PreZ_None,
	Cull_Deferred_Volume,
	Cull_Deferred_Quad,
	Cull_Deferred_Tile,
};

enum LightingMethod
{
	Lighting_Forward,
	Lighting_Deferred,
};

class SSAO
{
public:
	SSAO(ID3D11Device* d3dDevice);
	~SSAO(void);

	void SetLightingMethod(LightingMethod method) { mLightingMethod = method; }
	void SetLightCullTechnique(LightCullTechnique tech) { mCullTechnique = tech; }
	void SetLightPrePass(bool b) { mLighting = b; }

	void OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferDesc);

	void Render(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
		const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	/**
	 * Forward only render the directional light
	 */
	void RenderForward(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
		const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void RenderDeferred(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
		const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

private:

	void RenderGBuffer(ID3D11DeviceContext* d3dDeviceContext, const Scene& scene, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void ComputeShading(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void RenderSSAO(ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void DrawPointLight(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void DrawSpotLight(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void DrawDirectionalLight(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void DrawLightVolumeDebug(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void PostProcess(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth, const D3D11_VIEWPORT* viewport);

	void EdgeAA(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, const D3D11_VIEWPORT* viewport);

private:
	
	LightingMethod mLightingMethod;
	LightCullTechnique mCullTechnique;

	bool mLighting;

	UINT mGBufferWidth, mGBufferHeight; 

	ID3D11InputLayout* mMeshVertexLayout;
	ID3D11InputLayout* mLightProxyVertexLayout;

	ID3D11Buffer* mPerFrameConstants;
	ID3D11Buffer* mPerObjectConstants;
	ID3D11Buffer* mAOParamsConstants;
	ID3D11Buffer* mLightConstants;

	ID3D11Buffer* mStreamOutputGPU;
	ID3D11Buffer* mStreamOutputCPU;

	shared_ptr<Texture2D> mDepthBuffer;
	ID3D11DepthStencilView* mDepthBufferReadOnlyDSV;

	std::vector<shared_ptr<Texture2D>> mGBuffer;
	std::vector<ID3D11RenderTargetView*> mGBufferRTV;
	std::vector<ID3D11ShaderResourceView*> mGBufferSRV;

	// Deferred Shading Lit Buffer
	shared_ptr<Texture2D> mLightAccumulateBuffer;
	shared_ptr<Texture2D> mLitBuffer;
	shared_ptr<Texture2D> mAOBuffer;

	// GBuffer Shaders
	shared_ptr<VertexShader> mGBufferVS;
	shared_ptr<PixelShader> mGBufferPS;

	// forward shader
	shared_ptr<VertexShader> mForwardDirectionalVS;
	shared_ptr<PixelShader> mForwardDirectionalPS;

	// classic deferred shading, for each light type
	shared_ptr<VertexShader> mDeferredShadingVS[3];
	shared_ptr<PixelShader> mDeferredShadingPS[3];

	// deferred lighting, lighting pass
	shared_ptr<PixelShader> mDeferredLightingPS[3];

	// deferred lighting, shading pass
	shared_ptr<PixelShader> mDeferredLightingShadingPS;

	shared_ptr<VertexShader> mScreenQuadVS;
	shared_ptr<GeometryShader> mScreenQuadGS;

	shared_ptr<VertexShader> mDebugVS;;
	shared_ptr<PixelShader> mDebugPS;

	shared_ptr<PixelShader> mEdgeAAPS;

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
	ID3D11DepthStencilState* mVolumeStencilState;  // Stencil Pass for local light volume optimization

	ID3D11BlendState* mGeometryBlendState;
	ID3D11BlendState* mLightingBlendState;

	CDXUTSDKMesh* mPointLightProxy;
	CDXUTSDKMesh* mSpotLightProxy;

	shared_ptr<PixelShader> mFullQuadSprite;
};

