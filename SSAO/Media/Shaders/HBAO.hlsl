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
	float2 InvFocalLen;

	float2 ProjScale; //(M33, M43)
};

SamplerState PointSampler     : register(s0);
SamplerState PointWrapSampler : register(s1);

Texture2D DepthBuffer   : register(t0);   // ZBuffer
Texture2D RandomTexture : register(t1);   // (cos(alpha),sin(alpha),jitter)    

float LengthSquared(float3 v)
{
	return dot(v, v);
}

//----------------------------------------------------------------------------------
float3 MinDiff(float3 P, float3 Pr, float3 Pl)
{
    float3 V1 = Pr - P;
    float3 V2 = P - Pl;
    return (dot(V1, V1) < dot(V2, V2)) ? V1 : V2;
}

//----------------------------------------------------------------------------------
float Falloff(float d2)
{
    // 1 scalar mad instruction
    return d2 * g_NegInvR2 + 1.0f;
}

//----------------------------------------------------------------------------------
float2 SnapUVOffset(float2 uv)
{
    return round(uv * g_AOResolution) * g_InvAOResolution;
}

//----------------------------------------------------------------------------------
float TanToSin(float x)
{
    return x * rsqrt(x*x + 1.0f);
}

//----------------------------------------------------------------------------------
float3 TangentVector(float2 deltaUV, float3 dPdu, float3 dPdv)
{
    return deltaUV.x * dPdu + deltaUV.y * dPdv;
}

//----------------------------------------------------------------------------------
float Tangent(float3 T)
{
    return -T.z * InvLength(T.xy);
}

//----------------------------------------------------------------------------------
float BiasedTangent(float3 T)
{
    // Do not use atan() because it gets expanded by fxc to many math instructions
    return Tangent(T) + g_TanAngleBias;
}

float2 MinDiff(float2 P, float2 Pr, float2 Pl)
{
    float2 V1 = Pr - P;
    float2 V2 = P - Pl;
    return (dot(V1,V1) < dot(V2,V2)) ? V1 : V2;
}

float2 RotateDirection(float2 dir, float2 consin)
{
	return float2(consin.x * dir.x - consin.y * dir.y, 
	              consin.y * dir.x + consin.x * dir.y);
}

float3 FetchEyePos(float2 uv)
{
	//const float ProjM33 = ProjScale.x;
	//const float ProjM43 = ProjScale.y;
	//float eyeZ = ProjM43 / (DepthBuffer.Sample(PointSampler, uv).x - ProjM33);

	float eyeZ = ProjScale.y / (DepthBuffer.Sample(PointSampler, uv).x - ProjScale.x);

	// Convet UV to Clip Space
	uv =  uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

    return float3(uv * InvFocalLen * eyeZ, eyeZ );
}

void ComputeSteps(in float pixelRadius, in float rand, out float numSteps, out float2 uvStepSize)
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

float HorizonOcclusion(float2 deltaUV,
                       float2 texelDeltaUV,
                       float2 uv0,
                       float3 P,
                       float numSteps,
                       float randstep,
                       float3 dPdu,
                       float3 dPdv )
{
	// Randomize starting point within the first sample distance
    float2 uv = uv0 + SnapUVOffset( randstep * deltaUV );

	deltaUV = SnapUVOffset( deltaUV );

	// Compute tangent vector using the tangent plane
    float3 T = deltaUV.x * dPdu + deltaUV.y * dPdv;

	float tanH = BiasedTangent(T);

#if SAMPLE_FIRST_STEP
    // Take a first sample between uv0 and uv0 + deltaUV
    float2 snapped_duv = SnapUVOffset( randstep * deltaUV + texelDeltaUV );
    ao = IntegerateOcclusion(uv0, snapped_duv, P, dPdu, dPdv, tanH);
    --numSteps;
#endif

	float sinH;
	
	for(float i = 0; i <= numSteps; ++i)
	{
		uv += deltaUV;

		float3 S = FetchEyePos(iTex, viewRay);
		float tanS = Tangent(P, S);
        float d2 = LengthSquared(P-S);

		if (d2 < RadiusSquared && tanS > tanH)
		{
			 // Accumulate AO between the horizon and the sample
            float sinS = tanS / sqrt(1.0f + tanS*tanS);
            ao += Falloff(d2) * (sinS - sinH);

			// Update the current horizon angle
            tanH = tanS;
            sinH = sinS;
		}
	}
}



float4 HBAO(in float4 iPos : SV_Position,
			in float2 iTex : TEXCOORD0) :SV_Target0
{
	float3 P = FetchEyePos(iTex, iViewRay);

	// (cos(alpha),sin(alpha),jitter)
    float3 rand = tRandom.Sample(PointWrapSampler, iPos.xy / RANDOM_TEXTURE_WIDTH);

	float2 foucuLegth = (Projection._M11, Projection._M22); 

	float2 uvRadius = Radius * foucuLegth / P.z;
	float pixelRadius = uvRadius.x * AOResolution.x;

	if(screenRadius < 1) return 1.0;

	float numStep;
	float2 uvStepSize;
	ComputeSteps(pixelRadius, rand.z, numStep, uvStepSize);

	// Nearest neighbor pixels on the tangent plane
	float Pl, Pr, Pb, Pt;
	pl = FetchEyePos(iTex + float2(-InvAOResolution.x, 0), iViewRay);
	pr = FetchEyePos(iTex + float2(InvAOResolution.x, 0),  iViewRay);
	Pb = FetchEyePos(iTex + float2(0, -InvAOResolution.y), iViewRay);
	Pt = FetchEyePos(iTex + float2(0, InvAOResolution.y),  iViewRay);

	float3 dPdu = MinDiff(P, rightP, leftP);
	float3 dPdv = MinDiff(P, topP, bottpmP) * (AOResolution.y * InvAOResolution.x);

	//float3 dPdu = ddx(P);
	//float3 dPdv = ddx(P);

	float alpha = 2.0f * M_PI / NUM_DIRECTIONS;

	for (float d = 0; d < NUM_DIRECTIONS; ++d)
    {
         float angle = alpha * d;
         float2 dir = RotateDirections(float2(cos(angle), sin(angle)), rand.xy);
         float2 deltaUV = dir * uvStepSize.xy;

         float2 texelDeltaUV = dir * g_InvAOResolution;


         ao += HorizonOcclusion(deltaUV, texelDeltaUV, IN.texUV, P, numSteps, rand.z, dPdu, dPdv);
    }
}

#endif
