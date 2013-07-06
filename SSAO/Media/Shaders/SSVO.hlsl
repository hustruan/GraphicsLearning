
cbuffer AmbientOcclusionConstant : register(b0)
{
	float2 AOResolution;
	float2 InvAOResolution;

	float2 FocalLen;
	float2 ClipInfo;

	float Radius;
	float RadiusSquared;
	float InvRadiusSquared;
	float MaxRadiusPixels;

	float TanAngleBias;
	float Strength;
};

#define RANDOM_TEXTURE_WIDTH 4

SamplerState PointClampSampler : register(s0);
SamplerState PointWrapSampler  : register(s1);

Texture2D<float> DepthBuffer    : register(t0);   // ZBuffer
Texture2D<float3> RandomTexture : register(t1);   // (cos(alpha),sin(alpha),jitter)   


float ViewDepth(float2 iTex)
{
	float eyeZ = ClipInfo.y / (DepthBuffer.Sample(PointClampSampler, uv) - ClipInfo.x);
}

float4 HBAO(in float4 iPos : SV_Position, in float2 iTex : TEXCOORD0) :SV_Target0
{
	// (cos(alpha),sin(alpha),jitter)
	float3 rand = RandomTexture.Sample(PointWrapSampler, iTex * AOResolution / RANDOM_TEXTURE_WIDTH);

	float2x2 rotation = float2x2(rand.x, rand.y
	                            -rand.y, rand.x);

	
	// Count this as our first sample point:
	// We know that it's half occluded by definition
	float occlusionAmount = 0.5f * centerWeight;

}

