#ifndef AOConstant_HLSL
#define AOConstant_HLSL

cbuffer AmbientOcclusionConstant
{
	float2 AOResolution;
	float2 InvAOResolution;

	float2 FocalLen; // (cot(fov/2)/aspect, cot(fov/2)) == (Proj._11, Proj._22)
	float2 ClipInfo; // (Proj._33, Proj._43) 

	float Radius;
	float RadiusSquared;
	float InvRadiusSquared;
	float MaxRadiusPixels;

	float TanAngleBias;
	float Strength;
};

#endif