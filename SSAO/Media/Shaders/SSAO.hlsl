#ifndef SSAO_HLSL
#define SSAO_HLSL

#include "FullScreenTriangle.hlsl"
#include "GBuffer.hlsl"

cbuffer SSAOParams
{
	// x for width, y for height, z for camera near, w for camera far
	float4 ViewParams;
	float4 AOParams;
};

// xyz for normal, w for depth
Texture2D GBufferTexture : register( t0 );
Texture2D NoiseTexture : register( t1 );

SamplerState GBufferSampler : register(s0);
SamplerState NoiseSampler : register(s1);

float4 SSAOPS(FullScreenTriangleVSOut input) : SV_Target0
{
	const int NumSamples = 16;

	float3 SampleKernel[NumSamples] =
	{
      float3( 0.5381, 0.1856,-0.4319), float3( 0.1379, 0.2486, 0.4430),
      float3( 0.3371, 0.5679,-0.0057), float3(-0.6999,-0.0451,-0.0019),
      float3( 0.0689,-0.1598,-0.8547), float3( 0.0560, 0.0069,-0.1843),
      float3(-0.0146, 0.1402, 0.0762), float3( 0.0100,-0.1924,-0.0344),
      float3(-0.3577,-0.5301,-0.4358), float3(-0.3169, 0.1063, 0.0158),
      float3( 0.0103,-0.5869, 0.0046), float3(-0.0897,-0.4940, 0.3287),
      float3( 0.7119,-0.0154,-0.0918), float3(-0.0533, 0.0596,-0.5411),
      float3( 0.0352,-0.0631, 0.5460), float3(-0.4776, 0.2847,-0.0271)
   };

   const float2 ScreenSize = ViewParams.xy; 
   const float FarPlane = ViewParams.w;
   const float Radius = AOParams.y;
   const float DefalutAO = AOParams.x;
   
   float3 random = 2.0 * NoiseTexture.Sample(NoiseSampler, input.oTex * ScreenSize / 4) - 1.0;

   float4 texel = GBufferTexture.Sample(GBufferSampler, input.oTex);
   float3 pixelNormal = DecodeNormal(texel.xyz);
   float pixelDepth = texel.a;

   	// Specified in either texels or meters
   float viewDepth = pixelDepth * FarPlane;
   float3 kernelScale = float3(Radius / viewDepth, Radius / viewDepth, Radius / ViewParams.w);

   float occlusion = 0.0; 
   for(int i = 0; i < NumSamples; ++i)
   {
       float3 rotatedKernel = reflect( SampleKernel[i], random ) * kernelScale;
	   float sampleDepth = GBufferTexture.Sample(GBufferSampler, input.oTex + rotatedKernel.xy).a;

	   float delta = max( sampleDepth - (pixelDepth + rotatedKernel.z), 0.0f );
	   float range = abs( delta ) / ( kernelScale.z * AOParams.z );
	   occlusion += lerp( ( delta * AOParams.w ), DefalutAO, saturate( range ) );
   }
   	
   occlusion /= NumSamples;
   
   occlusion = lerp( 0.1f, 0.6, saturate( occlusion ) );

   return float4(occlusion, occlusion, occlusion, 1);
}


#endif