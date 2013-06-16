#ifndef DeferredRenderingWS_HLSL
#define DeferredRenderingWS_HLSL

#include "Utility.hlsl"

cbuffer PerFrameConstants : register(b0)
{
	float4x4 World;
    float4x4 ViewProj;
	float4x4 InvViewProj;
	float4 CameraPosition;
	float4 CameraZAxis;
    float4 CameraNearFar;
	uint4 FramebufferDimensions;
};

cbuffer Light  : register(b1)
{
	float3 LightColor;
	float3 LightPos;       
	float3 LightDirection; 
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

	float4 posWS = mul(oPos, InvViewProj);
	posWS /= posWS.w;
	
#elif defined(PointLight) || defined(SpotLight)
	
	float4 posWS = mul(float4(iPos, 1.0f), World);

	oPos = mul(posWS, ViewProj);
	oTex = float3(oPos.xy, oPos.w);

#endif

	oViewRay = posWS.xyz - CameraPosition; 
}


float4 DeferredRenderingPS(
#if defined(Quad)       // Directional light
	in float2 iTex : TEXCOORD0,
#elif defined(Volume)   // Point or Spot light
    in float3 iTex : TEXCOORD0,
#endif
	in float3 iViewRay : TEXCOORD1)  : SV_Target0

{
#if defined(DirectionalLight)
	float2 UV = iTex;
#elif defined(PointLight) || defined(SpotLight)
	float2 UV = ConvertUV( iTex.xy / iTex.z );
#endif

	float3 viewRay = normalize(iViewRay);

	// Convert non-linear depth to view space linear depth
	float linearDepth = LinearizeDepth( DepthBuffer.Sample(PointSampler, tex).x, CameraNearFar.x, CameraNearFar.y );

	// Project the view ray onto the camera's z-axis
	float viewZProj = dot(CameraZAxis, viewRay);

	// Scale the view ray by the ratio of the linear z value to the projected view / ray
	float3 positionWS = CameraPosition + viewRay * (linearDepth / viewZProj);

	float3 L = 0;
	float attenuation = 1.0f;

#if defined(PointLight) || defined(SpotLight)
	L = LightPos - positionWS;

	float dist = length(L);
	attenuation = CalculateAttenuation(dist, LightFalloff.x, LightFalloff.y);
	
	L /= dist;

#elif defined(DirectionalLight)
	L = -LightDirection;
#endif

#if defined(SpotLight)
	float cosAngle = dot( -L, LightDirection ); 
	attenuation *= CalculateSpotCutoff(cosAngle, SpotFalloff.x, SpotFalloff.y, SpotFalloff.z);
#endif

	// Fetch GBuffer
	float4 tap0 = GBuffer0.Sample(PointSampler, tex);
	
	// Decode view space normal
	float3 N = normalize(DecodeNormal( tap0.rgb )); 
	float shininess = tap0.a * 256.0f;

	float nDotl = dot(L , N);

	if(nDotl > 0)
	{
		float3 V = normalize(CameraPosition - positionWS);
		float3 H = normalize(L + V);

		float4 tap1 = GBuffer1.Sample(PointSampler, tex);

		// Get Diffuse Albedo and Specular
		float3 diffuseAlbedo = tap1.rgb;
		float3 specularAlbedo = tap1.aaa;

		final = diffuseAlbedo + CalculateFresnel(specularAlbedo, L, H) * CalculateSpecular(N, H, shininess);
		final *= LightColor * nDotl * attenuation;
	}

	return final;
}

#endif
