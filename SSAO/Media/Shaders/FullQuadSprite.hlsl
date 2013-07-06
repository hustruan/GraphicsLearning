#ifndef FullQuadSprite_HLSL
#define FullQuadSprite_HLSL

#include "FullScreenTriangle.hlsl"

SamplerState PointSampler : register(s0);
Texture2D SpriteTex : register(t0);      

float4 FullQuadSpritePS(FullScreenTriangleVSOut input) : SV_Target0
{
	float4 albedo = SpriteTex.Sample(PointSampler, input.oTex);
	return float4(albedo.rgb, 1.0);
}

float4 FullQuadSpriteAOPS(FullScreenTriangleVSOut input) : SV_Target0
{
	float4 ao = SpriteTex.Sample(PointSampler, input.oTex).x;
	return float4(ao.xxx, 1.0);
}

#endif