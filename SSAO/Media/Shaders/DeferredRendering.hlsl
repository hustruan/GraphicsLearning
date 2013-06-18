#ifndef DeferredRendering_HLSL
#define DeferredRendering_HLSL

#include "PerFrameConstants.hlsl"
#include "Utility.hlsl"

cbuffer Light
{
	float3 LightColor;
	float3 LightPosVS;        // View space light position
	float3 LightDirectionVS; 
	float3 SpotFalloff;
	float2 LightFalloff;   // begin and end
};

SamplerState PointSampler : register(s0);

Texture2D GBuffer0    : register(t0);           // Normal + Shininess
Texture2D GBuffer1    : register(t1);           // Diffuse Albedo + Specular 
Texture2D DepthBuffer : register(t2);

void DeferredRenderingVS(                   
#if defined(DirectionalLight)
						 in  uint vertexID   : SV_VertexID,
						 out float2 oTex     : TEXCOORD0,
#else
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

#else
	
	oPos = mul(float4(iPos, 1.0f), WorldViewProj);
	oTex = float3(oPos.xy, oPos.w);
	oViewRay = mul(float4(iPos, 1.0f), WorldView).xyz;   // view space positon

#endif

}

float4 DeferredRenderingPointPS(in float3 oTex : TEXCOORD0, in float3 oViewRay : TEXCOORD1) : SV_Target0
{
	float3 final = 0;

	float2 tex = ConvertUV( oTex.xy / oTex.z );
	
	// Convert non-linear depth to view space linear depth
	float linearDepth = LinearizeDepth( DepthBuffer.Sample(PointSampler, tex).x, CameraNearFar.x, CameraNearFar.y );

	// View space lit position
	float3 positionVS = PositionVSFromDepth(oViewRay, linearDepth);

	// Fetch GBuffer
	float4 tap0 = GBuffer0.Sample(PointSampler, tex);
	float4 tap1 = GBuffer1.Sample(PointSampler, tex);

	// Decode view space normal
	float3 N = normalize(DecodeNormal( tap0.rgb )); 
	float shininess = tap0.a * 256.0f;

	// Get Diffuse Albedo and Specular
	float3 diffuseAlbedo = tap1.rgb;
	float3 specularAlbedo = tap1.aaa;

	// light direction
	float3 L = LightPosVS - positionVS;
	float dist = length(L);
	L = normalize(L);

	float nDotl = dot(L , N);

	if(nDotl > 0)
	{
		float3 V = normalize(-positionVS);
		float3 H = normalize(L + V);
		final = diffuseAlbedo + CalculateFresnel(specularAlbedo, L, H) * CalculateSpecular(N, H, shininess);
		final *= LightColor * nDotl * CalculateAttenuation(dist, LightFalloff.x, LightFalloff.y);
	}

	return float4(final, 1.0f);
}

float4 DeferredRenderingDirectionPS(in float2 oTex : TEXCOORD0, in float3 oViewRay : TEXCOORD1) : SV_Target0
{
	float3 final = 0;

	// Convert non-linear depth to view space linear depth
	float linearDepth = LinearizeDepth( DepthBuffer.Sample(PointSampler, oTex).x, CameraNearFar.x, CameraNearFar.y );

	// View space lit position
	float3 positionVS = PositionVSFromDepth(oViewRay, linearDepth);

	// Fetch GBuffer
	float4 tap0 = 




	float4 tap1 = GBuffer1.Sample(PointSampler, oTex);

	// Decode view space normal
	float3 N = normalize(DecodeNormal( tap0.rgb )); 
	float shininess = tap0.a * 256.0f;

	// Get Diffuse Albedo and Specular
	float3 diffuseAlbedo = tap1.rgb;
	float3 specularAlbedo = tap1.aaa;

	// light direction
	float3 L = -LightDirectionVS;

	float nDotl = dot(L, N);

	if(nDotl > 0)
	{
		float3 V = normalize(-positionVS);
		float3 H = normalize(L + V);
	
		final = diffuseAlbedo + CalculateFresnel(specularAlbedo, L, H) * CalculateSpecular(N, H, shininess);
		final *= LightColor * nDotl;
	}

	return float4(final, 1.0f);
}

//----------------------------------------------------
void DebugPointLightVS(in float3 iPos : POSITION, out float4 oPos : SV_Position) 
{
	oPos = mul(float4(iPos, 1.0f), WorldViewProj);
}

float4 DebugPointLightPS() : SV_Target0
{
	return float4(LightColor, 1.0f);
}

#endif

