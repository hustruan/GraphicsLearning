#ifndef HBAO_HLSL
#define HBAO_HLSL

#define M_PI 3.14159265f

#define SAMPLE_FIRST_STEP 1
#define USE_NORMAL_FREE_HBAO 0

#define NUM_DIRECTIONS 8
#define NUM_STEPS 6
#define RANDOM_TEXTURE_WIDTH 4

cbuffer AmbientOcclusionConstant : register(b0)
{
	float2 AOResolution;
	float2 InvAOResolution;

	float2 FocalLen;
	float2 ProjDepthScale;

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

float LengthSquared(float3 v)
{
	return dot(v, v);
}

float Tangent(float3 T)
{
	return -T.z * rsqrt(dot(T.xy, T.xy));
}

float BiasedTangent(float3 T)
{
	return Tangent(T) + TanAngleBias;
}

float Tan2Sin(float t)
{
	return t * rsqrt(1.0f + t*t);
}

float3 MinDiff(float3 P, float3 Pr, float3 Pl)
{
    float3 V1 = Pr - P;
    float3 V2 = P - Pl;
    return (dot(V1, V1) < dot(V2, V2)) ? V1 : V2;
}

float Falloff(float d2)
{
	// 1 - R*R
    return 1.0f - (d2 * InvRadiusSquared );
}

float2 SnapUVOffset(float2 uv)
{
    return round(uv * AOResolution) * InvAOResolution;
}

float2 RotateDirection(float2 dir, float2 consin)
{
    return float2(dir.x*consin.x - dir.y*consin.y,
                  dir.x*consin.y + dir.y*consin.x);
}

float3 FetchEyePos(float2 uv)
{
	//const float ProjM33 = ProjScale.x;
	//const float ProjM43 = ProjScale.y;
	//float eyeZ = ProjM43 / (DepthBuffer.SampleLevel(PointClampSampler, uv, 0) - ProjM33);

	float eyeZ = ProjDepthScale.y / (DepthBuffer.SampleLevel(PointClampSampler, uv, 0) - ProjDepthScale.x);

	// Convet UV to Clip Space
	uv =  uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

    return float3(uv / FocalLen * eyeZ, eyeZ );
}

void ComputeSteps(in float pixelRadius, in float rand, inout float numSteps, inout float2 uvStepSize)
{
	// Avoid oversampling if NUM_STEPS is greater than the kernel radius in pixels
    numSteps = min(NUM_STEPS, pixelRadius);

    // Divide by Ns+1 so that the farthest samples are not fully attenuated
    float pixelStepSize= pixelRadius / (numSteps + 1);

    // Clamp numSteps if it is greater than the max kernel footprint
    float maxNumSteps = MaxRadiusPixels / pixelStepSize;
    if (maxNumSteps < numSteps)
    {
        // Use dithering to avoid AO discontinuities
        numSteps = floor(maxNumSteps + rand);
        numSteps = max(numSteps, 1);
        pixelStepSize = MaxRadiusPixels / numSteps;
    }

    // Step size in uv space
    uvStepSize = pixelStepSize * InvAOResolution;
}


float IntegerateOcclusion(float2 uv0, float2 snapped_duv, float3 P, float3 dPdu, float3 dPdv, inout float tanH) 
{
	float ao = 0;

	// Compute a tangent vector for snapped_duv
	float3 T1 = snapped_duv.x * dPdu + snapped_duv.y * dPdv;
	
	float tanT = BiasedTangent(T1); 
	float sinT = Tan2Sin(tanT);

	float3 S = FetchEyePos(uv0 + snapped_duv);

	float tanS = Tangent(S-P);
	float sinS = Tan2Sin(tanS);

	float d2 = LengthSquared(S-P);

	[branch]
	if ( (d2 < RadiusSquared) && (tanS > tanH) )
	{
		ao = Falloff(d2) * (sinS - sinT);
		
		// Update the horizon angle
        tanH = max(tanH, tanS);
	}

	return ao;
}

float HorizonOcclusion(float2 deltaUV, float2 texelDeltaUV, float2 uv0,
                       float3 P, float3 dPdu, float3 dPdv, float numSteps, float randstep)
{
	float ao = 0;

	// Randomize starting point within the first sample distance
    float2 uv = uv0 + SnapUVOffset( randstep * deltaUV );

	deltaUV = SnapUVOffset( deltaUV );

	// Compute tangent vector using the tangent plane
    float3 T = deltaUV.x * dPdu + deltaUV.y * dPdv;

	float tanH = BiasedTangent(T);

#if SAMPLE_FIRST_STEP
    // Take a first sample between uv0 and uv0 + deltaUV
    float2 snapped_duv = SnapUVOffset( randstep * deltaUV + texelDeltaUV);
    ao = IntegerateOcclusion(uv0, snapped_duv, P, dPdu, dPdv, tanH);
    --numSteps;
#endif

	float sinH;
	
	for(float i = 1; i <= numSteps; ++i)
	{
		uv += deltaUV;

		float3 S = FetchEyePos(uv);
		float tanS = Tangent(S-P);
        float d2 = LengthSquared(S-P);

		[branch]
		if ( (d2 < RadiusSquared) && (tanS > tanH) )
		{
			 // Accumulate AO between the horizon and the sample
            float sinS = Tan2Sin(tanS);
            ao += Falloff(d2) * (sinS - sinH);

			// Update the current horizon angle
            tanH = tanS;
            sinH = sinS;
		}
	}

	return ao;
}

float4 HBAO(in float4 iPos : SV_Position, in float2 iTex : TEXCOORD0) :SV_Target0
{
	float3 P = FetchEyePos(iTex);

	// (cos(alpha),sin(alpha),jitter)
    //float3 rand = RandomTexture.Sample(PointWrapSampler, iPos.xy / RANDOM_TEXTURE_WIDTH);
	float3 rand = RandomTexture.Sample(PointWrapSampler, iTex * AOResolution / RANDOM_TEXTURE_WIDTH);

	float2 uvRadius = 0.5 * Radius * FocalLen / P.z;
	float pixelRadius = uvRadius.x * AOResolution.x;
	if(pixelRadius < 1) return 1.0;

	float numStep;
	float2 uvStepSize;
	ComputeSteps(pixelRadius, rand.z, numStep, uvStepSize);

	// Nearest neighbor pixels on the tangent plane
	float3 Pl, Pr, Pb, Pt;
	Pl = FetchEyePos(iTex + float2(-InvAOResolution.x, 0));
	Pr = FetchEyePos(iTex + float2(InvAOResolution.x, 0));
	Pb = FetchEyePos(iTex + float2(0, -InvAOResolution.y));
	Pt = FetchEyePos(iTex + float2(0, InvAOResolution.y));

	float3 dPdu = MinDiff(P, Pr, Pl);
	float3 dPdv = MinDiff(P, Pt, Pb) * (AOResolution.y * InvAOResolution.x);

	//float3 dPdu = ddx(P);
	//float3 dPdv = ddy(P);

	float ao = 0;
	float alpha = 2.0f * M_PI / NUM_DIRECTIONS;

	for (float d = 0; d < NUM_DIRECTIONS; ++d)
    {
        float angle = alpha * d;
        float2 dir =  RotateDirection(float2(cos(angle), sin(angle)), rand.xy);
        float2 deltaUV = dir * uvStepSize.xy;
		float2 texelDeltaUV = dir * InvAOResolution;

        ao += HorizonOcclusion(deltaUV, texelDeltaUV, iTex, P, dPdu, dPdv, numStep, rand.z);
    }

	ao = 1.0 - ao / NUM_DIRECTIONS * Strength;
	
	return float4(ao.xxx, 1.0);
}

#endif
