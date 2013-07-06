
#include "AOConstant.hlsl"

SamplerState PointClampSampler     : register(s0);
SamplerState PointWrapSampler      : register(s1);

Texture2D<float>  DepthBuffer      : register(t0);
Texture2D<float3> NoiseTexture     : register(t1);

#define RANDOM_TEXTURE_WIDTH 4

float ViewDepth(float2 uv)
{
	return ClipInfo.y / (DepthBuffer.SampleLevel(PointClampSampler, uv, 0) - ClipInfo.x);
}

float CryteckSSAO(float4 iPos : SV_Position, float2 iTex : TEXCOORD0) : SV_Target0
{
	float3 random = 2.0f * NoiseTexture.Sample(PointWrapSampler, iPos.xy / RANDOM_TEXTURE_WIDTH).rgb - 1.0f;

	// View space depth
	float fSceneDepthP = ViewDepth(iTex);

	// Parameters affecting offset points numbers and distribution
	const int NumSamples = 16;
	float offsetScale = 0.002;
	//float offsetScale = 0.5 * FocalLen.y * Radius / fSceneDepthP;
	float offsetScaleStep = 1 + 2.4 / NumSamples;

	float Accessibility = 0;

	for (int i = 0; i < (NumSamples/8); ++i)
	for (int x = -1; x <= 1; x +=2)
	for (int y = -1; y <= 1; y +=2)
	for (int z = -1; z <= 1; z +=2)
	{
		float3 vOffset = normalize(float3(x, y, z)) * (offsetScale *= offsetScaleStep);
		
		// rotate offset vector
		float3 vRotatedOffset = reflect(vOffset, random);

		float3 vSamplePos = float3(iTex, fSceneDepthP);

		// shift coordinates by offset vector (range convert and width depth value)
		vSamplePos += float3(vRotatedOffset.xy, vRotatedOffset.z * fSceneDepthP * 2);

		float fSceneDepthS = ViewDepth(vSamplePos.xy);

		// check if depth of both pixels are close enough and sampling point should affect out center pixel
		float fRangeIsValid = saturate( (fSceneDepthP - fSceneDepthS) / fSceneDepthS );

		Accessibility += lerp( fSceneDepthS > vSamplePos.z, 0.5, fRangeIsValid);
	}

	Accessibility = Accessibility / NumSamples;
	
	return saturate(Accessibility*Accessibility + Accessibility);
}