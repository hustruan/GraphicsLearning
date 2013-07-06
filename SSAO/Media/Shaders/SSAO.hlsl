#ifndef SSAO_HLSL
#define SSAO_HLSL

#include "PerFrameConstant.hlsl"
#include "FullScreenTriangle.hlsl"
#include "Utility.hlsl"

cbuffer SSAOParams : register(b1)
{
	float DefaultAO : packoffset(c0.x);
	float Radius    : packoffset(c0.y);
	float NormRange : packoffset(c0.z);
	float DeltaSacle : packoffset(c0.w);
};

SamplerState PointClampSampler     : register(s0);
SamplerState PointWrapSampler      : register(s1);

Texture2D GBuffer0      : register(t0);           // Normal + Shininess
Texture2D NoiseTexture  : register(t1);          // 4x4 texture containing 16 random vectors
Texture2D DepthBuffer   : register(t2);           // ZBuffer

float CryteckSSAO(float2 tex)
{
	float2 ScreenSize;
    GBuffer0.GetDimensions(ScreenSize.x, ScreenSize.y);

	float3 random = 2.0f * NoiseTexture.Sample(PointWrapSampler, tex * ScreenSize / 4).rgb - 1.0f;

	// Convert non-linear depth to view space linear depth
	float fSceneDepthP = LinearizeDepth( DepthBuffer.Sample(PointClampSampler, tex).x, CameraNearFar.x, CameraNearFar.y );

	// Parameters affecting offset points numbers and distribution
	const int NumSamples = 16;
	float offsetScale = 1.0;
	float offsetScaleStep = 1 + 2.4 / NumSamples;

	float accessibility = 0;

	for (int i = 0; i < (NumSamples/8); ++i)
	for (int x = -1; x <= 1; x +=2)
	for (int y = -1; y <= 1; y +=2)
	for (int z = -1; z <= 1; z +=2)
	{
		float3 vOffset = normalize(float3(x, y, z)) * (offsetScale *= offsetScaleStep);
		
		// rotate offset vector
		float3 vRotatedOffset = reflect(vOffset, random);

		float3 vSamplePos = float3(tex, fSceneDepthP);

		// shift coordinates by offset vector (range convert and width depth value)
		vSamplePos += float3(vRotatedOffset.xy, vRotatedOffset.z * fSceneDepthP * 2);
	
		float fSceneDepthS = LinearizeDepth( DepthBuffer.Sample(PointClampSampler, vSamplePos.xy).x, CameraNearFar.x, CameraNearFar.y );

		// check if depth of both pixels are close enough and sampling point should affect out center pixel
		float fRangeIsValid = saturate( (fSceneDepthP - fSceneDepthS) / fSceneDepthS );

		accessibility += lerp( fSceneDepthS > vSamplePos.z, 0.5, fRangeIsValid);
	}

	return accessibility / NumSamples;
}

float GPWikiSSAO(float2 tex)
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

   float2 ScreenSize;
   GBuffer0.GetDimensions(ScreenSize.x, ScreenSize.y);

   float3 random = 2.0 * NoiseTexture.Sample(PointWrapSampler, tex * ScreenSize / 4).rgb - 1.0;

   float viewDepth = LinearizeDepth( DepthBuffer.Sample(PointClampSampler, tex).x, CameraNearFar.x, CameraNearFar.y );
   float pixelDepth = viewDepth / CameraNearFar.y;

   float3 kernelScale = float3(Radius / viewDepth, Radius / viewDepth, Radius / CameraNearFar.y);

   float occlusion = 0.0; 
   for(int i = 0; i < NumSamples; ++i)
   {
       float3 rotatedKernel = reflect( SampleKernel[i], random ) * kernelScale;
	   float sampleDepth = LinearizeDepth( DepthBuffer.Sample(PointClampSampler, tex + rotatedKernel.xy).x, CameraNearFar.x, CameraNearFar.y ) / CameraNearFar.y;

	   float delta = max( sampleDepth - pixelDepth + rotatedKernel.z, 0.0f );
	   float range = abs( delta ) / ( kernelScale.z * NormRange );
	   occlusion += lerp( ( delta * DeltaSacle ), DefaultAO, saturate( range ) );
   }
   	
   occlusion /= NumSamples;
   
   occlusion = lerp( 0.1f, 0.6, saturate( occlusion ) );

   return occlusion;
}


float4 SSAOPS(FullScreenTriangleVSOut input) : SV_Target0
{
	//float ao = GPWikiSSAO(input.oTex);

	float ao = CryteckSSAO(input.oTex);
	
	return float4(ao.xxx, 1);
}

#endif