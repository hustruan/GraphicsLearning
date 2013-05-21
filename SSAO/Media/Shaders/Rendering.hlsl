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
    float3 oPosView   : TEXCOORD0;      // View space position
    float3 oNormal    : TEXCOORD1;
    float2 oTex       : TEXCOORD2;
};

GeometryVSOut GeometryVS(GeometryVSIn input)
{
    GeometryVSOut output;

    output.oPos     = mul(float4(input.iPos, 1.0f), WorldViewProj);
    output.oPosView = mul(float4(input.iPos, 1.0f), WorldView).xyz;
    output.oNormal  = mul(float4(input.iNormal, 0.0f), WorldView).xyz;
    output.oTex     = input.iTex;
    
    return output;
}

void GeometryPS(GeometryVSOut input, out GBuffer outputGBuffer)
{
    float4 albedo = DiffuseTexture.Sample(DiffuseSampler, input.oTex);

    float3 normal = normalize(input.oNormal);
     
	float normDepth = input.oPosView.z / CameraNearFar.y;
	 
    outputGBuffer.Normal = float4(EncodeNormal(normal), normDepth);
    outputGBuffer.Albedo = albedo;
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