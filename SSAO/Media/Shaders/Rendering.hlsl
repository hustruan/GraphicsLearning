#ifndef Rendering_HLSL
#define Rendering_HLSL

#include "PerFrameConstants.hlsl"
#include "GBuffer.hlsl"

Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);

struct GeometryVSIn
{
    float3 iPos       : POSITION;
    float3 iNormal    : NORMAL;
    float2 iTex       : TEXCOORD0;
};

struct GeometryVSOut
{
    float4 oPos       : SV_Position;
    float3 oNormal    : TEXCOORD0;
    float2 oTex       : TEXCOORD1;
};

float Shininess = 25.0f;
float Specular = 0.5f;

GeometryVSOut GeometryVS(GeometryVSIn input)
{
    GeometryVSOut output;

    output.oPos     = mul(float4(input.iPos, 1.0f), WorldViewProj);
    output.oNormal  = mul(float4(input.iNormal, 0.0f), WorldView).xyz;
    output.oTex     = input.iTex;
    
    return output;
}

void GeometryPS(GeometryVSOut input, out GBuffer outputGBuffer)
{
    float4 albedo = DiffuseTexture.Sample(DiffuseSampler, input.oTex);
    float3 normal = normalize(input.oNormal);
     
    outputGBuffer.Normal = float4(EncodeNormal(normal), Shininess/256.0f);
    outputGBuffer.Albedo = float4(albedo.rgb, Specular);
}

//---------------------------------------------------------------------------------
float4 BasicPS(GeometryVSOut input) : SV_Target0
{
	float4 albedo = DiffuseTexture.Sample(DiffuseSampler, input.oTex);
	
	float3 normal = normalize(input.oNormal);

	float3 light = normalize( mul(float4(0, 1, 0, 0), WorldView).xyz );

	float ndotl = dot(normal, light);

	float3 final = saturate(albedo.xyz * ndotl);
	
	return float4(final, 1.0);
}

#endif