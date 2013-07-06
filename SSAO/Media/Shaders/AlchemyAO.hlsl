
#define M_PI 3.14159265f
#define RANDOM_TEXTURE_WIDTH 4

// Total number of direct samples to take at each pixel
#define NUM_SAMPLES (9)

// This is the number of turns around the circle that the spiral pattern makes.  This should be prime to prevent
// taps from lining up.  This particular choice was tuned for NUM_SAMPLES == 9
#define NUM_SPIRAL_TURNS (7)

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

SamplerState PointClampSampler : register(s0);
SamplerState PointWrapSampler  : register(s1);

Texture2D<float> DepthBuffer    : register(t0);   // ZBuffer
Texture2D<float3> RandomTexture : register(t1);   // (cos(alpha),sin(alpha),jitter)    

static float Bias = 0.002f;

float3 FetchEyePos(float2 uv)
{
	float eyeZ = ClipInfo.y / (DepthBuffer.SampleLevel(PointClampSampler, uv, 0) - ClipInfo.x);

	// Convet UV to Clip Space
	uv =  uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

    return float3(uv / FocalLen * eyeZ, eyeZ );
}

float2 RotateDirection(float2 dir, float2 consin)
{
    return float2(dir.x*consin.x - dir.y*consin.y,
                  dir.x*consin.y + dir.y*consin.x);
}

float2 tapLocation(int sampleNumber, float spinAngle, out float ssR){
	// Radius relative to ssR
	float alpha = float(sampleNumber + 0.5) * (1.0 / NUM_SAMPLES);
	float angle = alpha * (NUM_SPIRAL_TURNS * 6.28) + spinAngle;

	ssR = alpha;
	return float2(cos(angle), sin(angle));
}


float4 AlchemyAO(in float4 iPos : SV_Position, in float2 iTex : TEXCOORD0) :SV_Target0
{
	// (cos(alpha),sin(alpha),jitter)
	float3 rand = RandomTexture.Sample(PointWrapSampler, iTex * AOResolution / RANDOM_TEXTURE_WIDTH);

	float3 C = FetchEyePos(iTex);
	float3 n_C = normalize(cross(ddx(C), ddy(C)));

	// Compute projection of disk of radius Radius into uv space
    // Multiply by 0.5 to scale from [-1,1]^2 to [0,1]^2
	float uvDiskRadius = 0.5 * FocalLen.y * Radius / C.z;

	int2 pixelPosC = int2(iPos.xy);

	// Hash function used in the HPG12 AlchemyAO paper
	float randomPatternRotationAngle = (3 * pixelPosC.x ^ pixelPosC.y + pixelPosC.x * pixelPosC.y) * 10;
	
	float alpha = 2.0f * M_PI / NUM_SAMPLES + randomPatternRotationAngle;

	float sum = 0;
	for(int i = 0; i < NUM_SAMPLES; ++i)
	{
		//float angle = alpha * i;
		//float2 unitDir =  RotateDirection(float2(cos(angle), sin(angle)), rand.xy);

		//float2 texS = iTex + /*normalize*/(unitDir) * rand.z * uvDiskRadius;
		
		// Offset on the unit disk, spun for this pixel
		float ssR;
		float2 unitOffset = tapLocation(i, randomPatternRotationAngle, ssR);
		ssR *= uvDiskRadius;

		float2 texS = iTex + (ssR*unitOffset);

		float3 Q = FetchEyePos(texS);
	
		float3 v = Q - C;

		float vv = dot(v, v);
		float vn = dot(v, n_C);

		const float epsilon = 0.01;

		float f = max(RadiusSquared - vv, 0.0); 
		sum += f * f * f * max((vn - Bias) / (epsilon + vv), 0.0);
	}

	float temp = RadiusSquared * Radius;
    sum /= temp * temp;

	float visibility = max(0.0, 1.0 - sum * (5.0 / NUM_SAMPLES));

	return float4(visibility.xxx, 1.0);
}
