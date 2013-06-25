#ifndef DeferredRendering_HLSL
#define DeferredRendering_HLSL

#if defined(DirectionalLight)

cbuffer PerFrameConstant : register(b0)
{
	float4x4 Projection;
	float4x4 InvProj;
	float4   CameraNearFar;
};

#elif defined(PointLight) || defined(SpotLight)   

cbuffer PerOjectConstant : register(b0)
{
	float4x4 WorldView;
	float4x4 WorldViewProj;
};

#endif


void DeferredRenderingVS(                   
#if defined(DirectionalLight)
						 in  uint vertexID   : SV_VertexID,
						 out float2 oTex     : TEXCOORD0,
#elif defined(PointLight) || defined(SpotLight)   
						 in  float3 iPos     : POSITION,
						 out float3 oTex     : TEXCOORD0, // divide by w is wrong, w may negative, So pass the clip space coord to pixel shader
#endif
						 out float3 oViewRay : TEXCOORD1,
						 out float4 oPos     : SV_Position)
{
#if defined(DirectionalLight)

	// Parametrically work out vertex location for full screen triangle
    float2 grid = float2((vertexID << 1) & 2, vertexID & 2);
    
	oPos = float4(grid * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    oTex = grid;

	float4 posVS = mul(oPos, InvProj);
	oViewRay = float3(posVS.xy / posVS.z, 1.0f);       // Proj to Z=1 plane

#elif defined(PointLight) || defined(SpotLight)   
	
	oPos = mul(float4(iPos, 1.0f), WorldViewProj);
	oTex = float3(oPos.xy, oPos.w);
	oViewRay = mul(float4(iPos, 1.0f), WorldView).xyz;   // view space positon

#endif

}

#endif

