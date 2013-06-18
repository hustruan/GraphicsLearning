#ifndef DeferredLighting_HLSL
#define DeferredLighting_HLSL

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
Texture2D DepthBuffer : register(t1);

// deferred lighting pass
float4 DeferredLightingPS(
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

	float4 final = 0;

	// Convert non-linear depth to view space linear depth
	float linearDepth = LinearizeDepth( DepthBuffer.Sample(PointSampler, tex).x, CameraNearFar.x, CameraNearFar.y );

	// View space lit position
	float3 positionVS = PositionVSFromDepth(iViewRay, linearDepth);

	float3 L = 0;
	float attenuation = 1.0f;

#if defined(PointLight) || defined(SpotLight)
	
	L = LightPosVS - positionVS;

	float dist = length(L);
	attenuation = CalculateAttenuation(dist, LightFalloff.x, LightFalloff.y);

	// Normailize
	L /= dist;

#elif defined(DirectionalLight)
	L = -LightDirectionVS;
#endif

#if defined(SpotLight)
	float cosAngle = dot( -L, LightDirectionVS ); 
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
		float3 diffuse = nDotl * LightColor * attenuation;

		float3 V = normalize(-positionVS);
		float3 H = normalize(L + V);

		float specular = CalculateSpecular(N, H, shininess) * attenuation * nDotl;

		final = float4(diffuse, specular);
	}

	return final;
}




#endif