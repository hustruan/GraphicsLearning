#ifndef DeferredRendering_HLSL
#define DeferredRendering_HLSL

#include "Utility.hlsl"

cbuffer PerFrameConstant : register(b0)
{
	float4x4 Projection;
	float4x4 InvProjection;
	float4   CameraNearFar;
};

cbuffer Light : register(b1)
{
	float3 LightColor;
	float3 LightPosition;        // View space light position
	float3 LightDirection; 
	float3 SpotFalloff;
	float2 LightAttenuation;   // begin and end
};

SamplerState PointSampler : register(s0);

Texture2D GBuffer0    : register(t0);           // Normal + Shininess
Texture2D GBuffer1    : register(t1);           // Diffuse Albedo + Specular 
Texture2D DepthBuffer : register(t2);

float4 DeferredRenderingPS(
#if defined(DirectionalLight)       
							in float2 iTex : TEXCOORD0,
#elif defined(PointLight) || defined(SpotLight)   
							in float3 iTex : TEXCOORD0,
#endif
							in float3 iViewRay : TEXCOORD1)  : SV_Target0
{
#if defined(DirectionalLight)
	float2 tex = iTex;
#elif defined(PointLight) || defined(SpotLight)
	float2 tex = ConvertUV( iTex.xy / iTex.z );
#endif

	float3 final = 0;

	// Convert non-linear depth to view space linear depth
	float linearDepth = LinearizeDepth( DepthBuffer.Sample(PointSampler, tex).x, CameraNearFar.x, CameraNearFar.y );

	// View space lit position
	float3 positionVS = PositionVSFromDepth(iViewRay, linearDepth);

	float3 L = 0;
	float attenuation = 1.0f;

#if defined(PointLight) || defined(SpotLight)
	
	L = LightPosition - positionVS;

	float dist = length(L);
	attenuation = CalculateAttenuation(dist, LightAttenuation.x, LightAttenuation.y);

	// Normailize
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

	float nDotl = dot(N, L);

	if(nDotl > 0)
	{
		float3 V = normalize(-positionVS);
		float3 H = normalize(L + V);

		float4 tap1 = GBuffer1.Sample(PointSampler, tex);

		// Get Diffuse Albedo and Specular
		float3 diffuseAlbedo = tap1.rgb;
		float3 specularAlbedo = tap1.aaa;

		final = diffuseAlbedo + CalculateFresnel(specularAlbedo, L, H) * CalculateSpecularNormalized(N, H, shininess);
		final *= LightColor * nDotl * attenuation;
	}

	//Additively blend is not free, discard pixel not contributing any light	  
	if(dot(final.rgb, 1.0) == 0) discard;

	return float4(final, 1.0f);
}

#endif

