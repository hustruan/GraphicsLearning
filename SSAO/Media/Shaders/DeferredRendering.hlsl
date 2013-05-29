#ifndef DeferredRendering_HLSL
#define DeferredRendering_HLSL

#include "PerFrameConstants.hlsl"
#include "Utility.hlsl"
#include "DeferredRead.hlsl"



cbuffer Light
{
	float3 LightPosVS;     // View space light position
};





float3 GetNormal()


void AccumulatePhongBRDF(float3 normal, float3 lightDir, float3 viewDir, float3 lightIntensity, float specularPower, 
                    inout float3 litDiffuse, inout float3 litSpecular)
{
	float NdotL = dot(normal, lightDir);

	if(NdotL > 0.0f)
	{
		float3 r = reflect(lightDir, normal);
		float RdotV = max(0.0f, dot(r, viewDir));

		litDiffuse += lightIntensity * NdotL;
		litSpecular += lightIntensity * pow(RdotV, specularPower);;
	}
}


void DeferredRenderingVS(in  float3 iPos     : POSITION,
                         out float4 oPos     : SV_Position,
						 out float2 oTex     : TEXCOORD0
						 out float3 oViewRay : TEXCOORD1)
{
#if defined(PointLight) || defined(SpotLight)
	
	oPos = mul(float4(iPos, 1.0f), WorldViewProj);

	// Convert [-1, 1]x[1, -1] to [0, 1]x[0, 1]
	oTex = oPos.xy / oPos.w;
	oTex = 0.5f * float2(oTex.x, -oTex.y) + float2(0.5f, 0.5f);

	oViewRay = mul(float4(iPos, 1.0f), WorldView).xyz;

#elif defined(DirectionalLight)  
	
	oPos = iPos;
	oTex = 0.5f * float2(iPos.x, -iPos.y) + float2(0.5f, 0.5f);

	float4 posVS = mul(float4(iPos, 1.0f), InvProj);
	oViewRay = float3(posVS.xy / posVS.z, 1.0f);

#endif 
}


void DeferredRenderingPointPS(in float2 oTex : TEXCOORD0, in float3 oViewRay : TEXCOORD1)
{
	float4 final = 0;

	float3 viewRay = float3(oViewRay.xy / oViewRay.z, 1.0f);
	
	// Convert non-linear depth to view space linear depth
	float linearDepth = LinearizeDepth( LinearDepthTex.Sample(PointSampler, oTex).r, CameraNearFar.x, CameraNearFar.y );

	// View space lit position
	float3 positionVS = viewRay * linearDepth;

	// Decode view space normal
	float3 normalVS = DecodeNormal( NormalTex.Sample(PointSampler, oTex).xyz ); 

	// light direction
	float3 lightDirVS = normalize(LightPosVS - positionVS);

	float nDotl = dot(lightDirVS, normalVS);

	if(nDotl > 0)
	{
		float4 tap =  AlbedoTex.Sample(PointSampler, oTex);

		float3 albedo = tap.rgb;
		float shininess = tap.a * 256.0f;


	}

}

void DeferredRenderingDirectionalPS£¨in float2 oTex : TEXCOORD0, in float3 oViewRay : TEXCOORD1)
{
	float3 viewRay = oViewRay;
}

#endif

