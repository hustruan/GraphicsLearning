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
Texture2D GBuffer1              : register(t1);
Texture2D DepthBuffer           : register(t2);
Texture2D LightAccumulateBuffer : register(t3);

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
		float3 diffuse = LightColor * nDotl * attenuation;

		float3 V = normalize(-positionVS);
		float3 H = normalize(L + V);

		// Frensel in moved to calculate in shading pass
		float3 specular = CalculateSpecular(N, H, shininess) * LightColor * nDotl * attenuation;

		final = float4(diffuse, Luminance(specular));
	}

	return final;
}

float4 DeferredShadingPS(in float2 iTex : TEXCOORD0, in float3 iViewRay : TEXCOORD1) : SV_Target0
{
	float3 final = 0;

	// Fetch GBuffer
	float4 tap0 = GBuffer0.Sample(PointSampler, iTex);
	float4 tap1 = GBuffer1.Sample(PointSampler, iTex);
	
	// Decode view space normal
	float3 N = normalize(DecodeNormal( tap0.rgb )); 
	float3 V = normalize(-iViewRay);

	// Get Diffuse Albedo and Specular
	float3 diffuseAlbedo = tap1.rgb;
	float3 specularAlbedo = tap1.aaa;
	float  specularPower = tap0.a * 256.0f;

	float4 lightColor = LightAccumulateBuffer.Sample(PointSampler, iTex);

	float3 diffueLight = lightColor.rgb;
	float3 specularLight = lightColor.a / (Luminance(lightColor.rgb) + 1e-6f) * lightColor.rgb;

	// Approximate fresnel by N and V
	float3 fresnelTerm = CalculateAmbiemtFresnel(specularAlbedo, N, V);
	
	final =  diffueLight * diffuseAlbedo + ((specularPower + 2.0f) / 8.0f) * fresnelTerm * specularLight;

	return float4(final, 1,0);
}

 

#endif