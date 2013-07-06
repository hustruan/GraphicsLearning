
#include "AOConstant.hlsl"

SamplerState PointClampSampler : register(s0);
SamplerState PointWrapSampler  : register(s1);

Texture2D<float> DepthBuffer    : register(t0);   // ZBuffer
Texture2D<float3> RandomTexture : register(t1);   // (cos(alpha),sin(alpha),jitter)   

#define M_PI 3.14159265f
#define RANDOM_TEXTURE_WIDTH 4

static const float bias = 0.008;

float3 FetchEyePos(float2 uv)
{
	float eyeZ = ClipInfo.y / (DepthBuffer.SampleLevel(PointClampSampler, uv, 0) - ClipInfo.x);
	uv =  uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    return float3(uv / FocalLen * eyeZ, eyeZ );
}

float acos_approximation(float x)
{
	return (-0.69813170079773212 * x * x - 0.87266462599716477) * x + 1.5707963267948966;
}

float CalcAngle(float2 sampleDir, float2 tex_C, float uvDiskRadius, float3 C, float3 n_C, inout float pairWeight)
{
	float2 tex_S = tex_C + sampleDir * uvDiskRadius;

	float3 S = FetchEyePos(tex_S);

	float3 V = normalize(-C);     // View
	float3 D = normalize(S - C);  
	
	float VdotS = dot(V, D);
	float NdotS = dot(n_C, D);

	float3 T = normalize(S - n_C * NdotS);  // Tangent

	float cosAngle = (NdotS >= 0) ? VdotS : dot(V, T); 

	//// Invalid samples are approximated by looking at the partner in the pair of samples
	float depthDiff = abs(S.z - C.z);
	//if (depthDiff > 20.0f)
	//{
	//	pairWeight -= 0.5;
	//	cosAngle = max(dot(V, -D), 0.0);
	//}

	return max( acos_approximation ( cosAngle - bias ) , 0.0) ;
}


float2 RotateDirection(float2 dir, float2 consin)
{
    return float2(dir.x*consin.x - dir.y*consin.y,
                  dir.x*consin.y + dir.y*consin.x);
}

float Unreal4AO(in float4 iPos : SV_Position, in float2 iTex : TEXCOORD0) :SV_Target0
{
	float3 C = FetchEyePos(iTex);
	float3 n_C = normalize(cross(ddx(C), ddy(C)));

	// (cos(alpha),sin(alpha),jitter)
	float3 rand = RandomTexture.Sample(PointWrapSampler, iPos.xy / RANDOM_TEXTURE_WIDTH);

	// Compute projection of disk of radius Radius into uv space
    // Multiply by 0.5 to scale from [-1,1]^2 to [0,1]^2
	float uvDiskRadius = 0.5 * FocalLen.y * Radius / C.z;

	const int NumSamples = 6;

	float ao = 0;
	float alpha = M_PI / NumSamples;

	float occlusion = 0;
	float weightSum = 0;
	for(int i = 0; i < NumSamples; ++i)
	{
		float angle = alpha * i;
        float2 dir =  RotateDirection(float2(cos(angle), sin(angle)), rand.xy);
		float offset = float(i + rand.z) / NumSamples;

		float pairWeight = 1.0;
		float angleSum = CalcAngle(dir, iTex, uvDiskRadius * offset, C, n_C, pairWeight) +
						 CalcAngle(-dir, iTex, uvDiskRadius * offset, C, n_C, pairWeight);
		
		occlusion += angleSum * pairWeight;
		weightSum += pairWeight;
	}

	occlusion /= weightSum * M_PI;

	return occlusion;
}