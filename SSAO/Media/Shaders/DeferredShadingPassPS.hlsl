#ifndef DeferredLighting_HLSL
#define DeferredLighting_HLSL

#include "PerFrameConstant.hlsl"
#include "Utility.hlsl"

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
Texture2D LightAccumulateBuffer : register(t2);
Texture2D AOBuffer              : register(t3);

// Deferred shading pass
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
	float3 specularLight = lightColor.a / (Luminance(diffueLight) + 1e-6f) * diffueLight;

	// Approximate fresnel by N and V
	float3 fresnelTerm = CalculateAmbiemtFresnel(specularAlbedo, N, V);
	
	final =  diffueLight * diffuseAlbedo + ((specularPower + 2.0f) / 8.0f) * fresnelTerm * specularLight;

	float ao = 1.0;
	if(UseSSAO)
		ao = AOBuffer.Sample(PointSampler, iTex).x;

	final = final + float3(0.2, 0.2, 0.2) * ao * diffuseAlbedo;

	return float4(final, 1.0);
}

 

#endif