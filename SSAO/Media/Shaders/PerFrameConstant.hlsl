#ifndef PerFrameConstant_HLSL
#define PerFrameConstant_HLSL

cbuffer PerFrameConstant : register(b0)
{
	float4x4 Projection;
	float4x4 InvProj;

	float2   CameraNearFar;

	bool     UseSSAO;
	bool     ShowAO;
};


#endif