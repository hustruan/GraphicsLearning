#ifndef DeferredRead_HLSL
#define DeferredRead_HLSL

SamplerState PointSampler : register(s0);

Texture2D GBuffer0 : register(t0);
Texture2D GBuffer1 : register(t1);
Texture2D DepthBuffer : register(t3);


float3 GetNormal(float2 texcoord)
{
	

}


#endif