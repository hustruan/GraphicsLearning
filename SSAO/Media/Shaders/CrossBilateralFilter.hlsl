#ifndef CrossBilateralFilter_HLSL
#define CrossBilateralFilter_HLSL

cbuffer CBFConstant : register(b0)
{
	float2 InvResolution;
	float  CameraNear;
	float  CameraFar;

	float BlurRadius;
	float BlurFalloff;
	float Sharpness;
};

SamplerState PointClampSampler   : register(s0);

Texture2D<float> DepthBuffer     : register(t0);
Texture2D<float> SourceBuffer    : register(t1);

// Convert to linear [0,1]
float FetchLinearDepth(float2 uv)
{
	float nonLinearDepth = DepthBuffer.Sample(PointClampSampler, uv);
	return CameraNear / (CameraFar - (CameraFar - CameraNear) * nonLinearDepth); 
}

float CrossBilateralWeight(float2 uv, float r, float d0, inout float totalWeight)
{
	float c = SourceBuffer.Sample(PointClampSampler, uv);
	
	float ddiff = FetchLinearDepth(uv) - d0;

	float w =  exp(-r*r*BlurFalloff - ddiff*ddiff*BlurFalloff);

	totalWeight += w;

	return w * c;
}

float BlurX(in float4 iPos : SV_Position, in float2 iTex : TEXCOORD0) : SV_Target0
{
	float totalWeight = 0;
	float b = 0;

    float centerDepth = FetchLinearDepth(iTex);

	for(float r = -BlurRadius; r <= BlurRadius; ++r)
	{
		float2 uv = iTex + r * float2(InvResolution.x, 0);
		b += CrossBilateralWeight(uv, r, centerDepth, totalWeight);
	}

	return b / totalWeight;
}

float4 BlurY(in float4 iPos : SV_Position, in float2 iTex : TEXCOORD0) : SV_Target0
{
	float totalWeight = 0;
	float b = 0;

    float centerDepth = FetchLinearDepth(iTex);

	for(float r = -BlurRadius; r <= BlurRadius; ++r)
	{
		float2 uv = iTex + r * float2(0, InvResolution.y);
		b += CrossBilateralWeight(uv, r, centerDepth, totalWeight);
	}

	float v =  b / totalWeight;

	return float4(v.xxx, 1.0);
}



#endif