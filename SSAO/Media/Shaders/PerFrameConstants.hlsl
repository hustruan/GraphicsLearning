#ifndef PerFrameConstants_HLSL
#define PerFrameConstants_HLSL

cbuffer PerFrameConstants : register(b0)
{
	float4x4 WorldViewProj;
    float4x4 WorldView;
    float4x4 Proj;
	float4x4 InvProj;
    float4 CameraNearFar;
	uint4 FramebufferDimensions;
};


#endif 