#ifndef CrossBilateralFilter_HLSL
#define CrossBilateralFilter_HLSL

SamplerState PointClampSampler   : register(s0);

Texture2D<float> DepthBuffer : register(t0);
Texture2D<float> AOBuffer    : register(t1);

float BlurX(in float4 oPos : SV_Position, in float2 oTex : TEXCOORD0) : SV_Target0
{
	


}


#endif