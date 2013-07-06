#pragma once

#include "DXUT.h"
#include "SDKmisc.h"
#include "Shader.h"
#include "ShaderContanst.h"
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

enum AmbientOcclusionTechnique
{
	AO_Cryteck = 0,
	AO_HBAO,
	AO_Unreal4,
	AO_Alchemy
};

class Renderer
{
public:
	Renderer(ID3D11Device* d3dDevice);
	~Renderer(void);

	void OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferDesc);

	void Render(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
		const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);
	
	// Only directional light
	void RenderForward(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
		const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void RenderDeferred(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth,
		const Scene& scene, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

private:

	void RenderGBuffer(ID3D11DeviceContext* d3dDeviceContext, const Scene& scene, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void ComputeShading(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void RenderCryteckSSAO(ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void RenderHBAO(ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void RenderAlchemyAO(ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);

	void RenderUnreal4AO(ID3D11DeviceContext* d3dDeviceContext, const CFirstPersonCamera& viewerCamera, const D3D11_VIEWPORT* viewport);
	
	void DrawPointLight(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void DrawSpotLight(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void DrawDirectionalLight(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void DrawLightVolumeDebug(ID3D11DeviceContext* d3dDeviceContext, const LightAnimation& lights, const CFirstPersonCamera& viewerCamera);

	void PostProcess(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, ID3D11DepthStencilView* backDepth, const D3D11_VIEWPORT* viewport);

	void EdgeAA(ID3D11DeviceContext* d3dDeviceContext, ID3D11RenderTargetView* backBuffer, const D3D11_VIEWPORT* viewport);


private:

	void CreateHBAORandomTexture(ID3D11Device* pDevice);

	void CreateShaderEffect(ID3D11Device* d3dDevice);

	void CreateConstantBuffers(ID3D11Device* d3dDevice);

	void CreateRenderStates(ID3D11Device* d3dDevice);

public:

	bool mLightPrePass;
	bool mUseSSAO;
	bool mShowAO;

	LightingMethod mLightingMethod;
	LightCullTechnique mCullTechnique;
	AmbientOcclusionTechnique mAOTechnique;

	float mAOOffsetScale;
	
	BlurParams mBlurParams;
	HBAOParams mHBAOParams;

private:
	
	UINT mGBufferWidth, mGBufferHeight; 

	ID3D11InputLayout* mMeshVertexLayout;
	ID3D11InputLayout* mLightProxyVertexLayout;

	ID3D11Buffer* mPerFrameConstants;
	ID3D11Buffer* mPerObjectConstants;
	ID3D11Buffer* mAOParamsConstants;
	ID3D11Buffer* mLightConstants;

	ID3D11Buffer* mHBAOParamsConstant;
	ID3D11Buffer* mBlurParamsConstants;

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
	shared_ptr<Texture2D> mBlurBuffer;

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
	shared_ptr<PixelShader> mHBAOPS;
	shared_ptr<PixelShader> mAlchemyAOPS;
	shared_ptr<PixelShader> mUnreal4AOPS;

	shared_ptr<PixelShader> mBlurXPS;
	shared_ptr<PixelShader> mBlurYPS;

	shared_ptr<PixelShader> mFullQuadSprite;
	shared_ptr<PixelShader> mFullQuadSpriteAO;

	ID3D11Texture2D* mHBAORandomTexture;
	ID3D11ShaderResourceView* mHBAORandomSRV;

	// Noise Texture
	ID3D11ShaderResourceView* mNoiseSRV;
	ID3D11ShaderResourceView* mBestFitNormalSRV;
	
	ID3D11SamplerState* mDiffuseSampler;
	ID3D11SamplerState* mPointWarpSampler;
	ID3D11SamplerState* mPointClampSampler;
	
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
};

