#ifndef DeferredLighting_HLSL
#define DeferredLighting_HLSL

#include "PerFrameConstant.hlsl"
#include "Utility.hlsl"

cbuffer Light : register(b1)
{
	float3 LightColor;
	float3 LightPosition;        // View space light position
	float3 LightDirection; 
	float3 SpotFalloff;
	float2 LightAttenuation;   // begin and end
};

SamplerState PointSampler : register(s0);

/**
 * @brief in Classic Deferred Lighting, GBuffer can only including MRT0(Normal+Shininess) + back depth buffer.
          in Lighting pass only access GBuffer0(Normal+Shininess), Shading pass will render scene geometry again,
		  read diffuse and specular material color from Texture, combine with lighting color which read from the 
		  light accumulation buffer.
		  
		  Instead of rendering scene geometry a second time in shading pass, we can just render a full screen pass.
		  In this method, GBuffer including MRT0(Normal+Shininess) + MRT1(diffuse albedo + specular albedo). So in 
		  final shading pass, we can read noraml, material color all from GBuffer, do lighting.
 */

Texture2D GBuffer0              : register(t0);           // Normal + Shininess
Texture2D DepthBuffer           : register(t1);           // Depth Buffer

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

	float nDotl = dot(L , N);

	if(nDotl > 0)
	{
		float3 diffuse = LightColor * nDotl * attenuation;

		float3 V = normalize(-positionVS);
		float3 H = normalize(L + V);

		// Frensel in moved to calculate in shading pass
		float3 specular = CalculateSpecular(N, H, shininess) * LightColor * nDotl * attenuation;

		final = float4(diffuse, Luminance(specular));
	}

	return final;
}

#endif