#include "Utility.hlsl"

// VS Constant
cbuffer PerOjectConstant : register(b0)
{
	float4x4 WorldView;
	float4x4 WorldViewProj;
};

// PS Constant
cbuffer Light : register(b0)
{
	float3 LightColor;
	float3 LightPosition;        // View space light position
	float3 LightDirection; 
	float3 SpotFalloff;
	float2 LightAttenuation;   // begin and end
};

Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);

struct ForwardVSIn
{
    float3 iPos       : POSITION;
    float3 iNormal    : NORMAL;
    float2 iTex       : TEXCOORD0;
};

struct ForwardVSOut
{
    float4 oPos       : SV_Position;
    float3 oNormal    : TEXCOORD0;
    float2 oTex       : TEXCOORD1;
	float3 oPosVS     : TEXCOORD2;
};

ForwardVSOut ForwardVS(ForwardVSIn input)
{
	ForwardVSOut output;

	output.oPos     = mul(float4(input.iPos, 1.0f), WorldViewProj);
	output.oPosVS   = mul(float4(input.iPos, 1.0f), WorldView).xyz;
    output.oNormal  = mul(float4(input.iNormal, 0.0f), WorldView).xyz;
    output.oTex     = input.iTex;

	return output;
}

float4 ForwardPS( ForwardVSOut input ) : SV_Target0
{
	float3 final = 0;

	const float shininess = 80.0f;
	const float3 specularAlbedo = float3(0.5f, 0.5f, 0.5f);

	float3 N = normalize(input.oNormal);
	float3 L = normalize(-LightDirection);
	
	float nDotl = dot(L, N);

	if(nDotl > 0)
	{	
		float3 V = normalize(-input.oPosVS);
		float3 H = normalize(L + V);
		
	    float3 diffuseAlbedo = DiffuseTexture.Sample(DiffuseSampler, input.oTex);

		
		final = diffuseAlbedo + CalculateFresnel(specularAlbedo, L, H) * CalculateSpecularNormalized(N, H, shininess);
		final *= LightColor * nDotl;
	}

	return float4(final, 1.0);
}