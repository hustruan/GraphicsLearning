#ifndef EdgeDetect_HLSL
#define EdgeDetect_HLSL

#include "FullScreenTriangle.hlsl"

Texture2D NormalTex    : register(t0);   
Texture2D DepthTex     : register(t1);
Texture2D ColorTex     : register(t2);

SamplerState PointSampler : register(s0);   
SamplerState LinearSampler : register(s1);   

const static float2 TexOffset[7] =  
{
	float2( 0.0f,  0.0f), // Center
	float2(-1.0f, -1.0f), // Top Left
	float2( 1.0f,  1.0f), // Bottom Right
	float2(-1.0f,  1.0f), // Top Right
	float2( 1.0f, -1.0f), // Bottom Left
	float2(-1.0f,  0.0f), // Left    
    float2( 0.0f, -1.0f)  // Top  
};

//cbuffer EdgeDetectCB
//{
//	float2 e_barrier = float2(0.8f,0.00001f);  // x=normal, y=depth
//	float2 e_weights= float2(1,1);             // x=normal, y=depth
//	float2 e_kernel = float2(1,1);             // x=normal, y=depth
//};

const static float2 e_barrier = float2(0.8f,0.000005f);  // x=normal, y=depth
const static float2 e_weights= float2(1,1);             // x=normal, y=depth
const static float2 e_kernel = float2(1,1);             // x=normal, y=depth


float4 EdgeDetectPS(FullScreenTriangleVSOut input) : SV_Target0
{
	float2 iTex = input.oTex;
	
	// Normal discontinuity filter
	
	float2 PixelSize;
    NormalTex.GetDimensions(PixelSize.x, PixelSize.y);
	PixelSize = float2(1.0f, 1.0f) / PixelSize;

	float3 nc = normalize(NormalTex.Sample(PointSampler, iTex + TexOffset[1] * PixelSize).xyz); 

	float4 nd;	
	nd.x =  dot(nc, normalize(NormalTex.Sample(PointSampler, iTex + TexOffset[1] * PixelSize).xyz));
	nd.y =  dot(nc, normalize(NormalTex.Sample(PointSampler, iTex + TexOffset[2] * PixelSize).xyz));
	nd.z =  dot(nc, normalize(NormalTex.Sample(PointSampler, iTex + TexOffset[3] * PixelSize).xyz));
	nd.w =  dot(nc, normalize(NormalTex.Sample(PointSampler, iTex + TexOffset[4] * PixelSize).xyz));

	nd -= e_barrier.x;
	nd = step(0, nd);
	float ne = saturate(dot(nd, e_weights.x));

	// Depth filter : compute gradiental difference:
	// (c-sample1)+(c-sample1_opposite)

	float dc = DepthTex.Sample(PointSampler, iTex).r;

	float4 dd;

	dd.x = DepthTex.Sample(PointSampler, iTex + TexOffset[1] * PixelSize).r +
           DepthTex.Sample(PointSampler, iTex + TexOffset[2] * PixelSize).r;

	dd.y = DepthTex.Sample(PointSampler, iTex + TexOffset[3] * PixelSize).r +
           DepthTex.Sample(PointSampler, iTex + TexOffset[4] * PixelSize).r;

	dd.z = DepthTex.Sample(PointSampler, iTex + TexOffset[5] * PixelSize).r +
           DepthTex.Sample(PointSampler, iTex - TexOffset[5] * PixelSize).r;

	dd.w = DepthTex.Sample(PointSampler, iTex + TexOffset[6] * PixelSize).r +
           DepthTex.Sample(PointSampler, iTex - TexOffset[6] * PixelSize).r;

	dd = abs(2 * dc - dd)- e_barrier.y;
	dd = step(dd, 0);

	float de = saturate(dot(dd, e_weights.y));

	// Weight
    float w = (1 - de * ne) * e_kernel.x; // 0 - no aa, 1=full aa

	w = ne;

	return float4(w, w, w, 1.0f);

	// Smoothed color
    // (a-c)*w + c = a*w + c(1-w)

    //float2 offset = iTex * (1-w);
    //float4 s0 = LinearSampler.Sample(LinearSampler, offset + (iTex + TexOffset[1] * PixelSize) * w);
    //float4 s1 = LinearSampler.Sample(LinearSampler, offset + (iTex + TexOffset[2] * PixelSize) * w);
    //float4 s2 = LinearSampler.Sample(LinearSampler, offset + (iTex + TexOffset[3] * PixelSize) * w);
    //float4 s3 = LinearSampler.Sample(LinearSampler, offset + (iTex + TexOffset[4] * PixelSize) * w);
    //return (s0 + s1 + s2 + s3) / 4.0f;
}


  ////////////////////////////  
   // Neighbor offset table  
   ////////////////////////////  
const static float2 offsets[9] = {  
  float2( 0.0,  0.0), //Center       0  
   float2(-1.0, -1.0), //Top Left     1  
   float2( 0.0, -1.0), //Top          2  
   float2( 1.0, -1.0), //Top Right    3  
   float2( 1.0,  0.0), //Right        4  
   float2( 1.0,  1.0), //Bottom Right 5  
   float2( 0.0,  1.0), //Bottom       6  
   float2(-1.0,  1.0), //Bottom Left  7  
   float2(-1.0,  0.0)  //Left         8  
}; 
 
float4 DL_GetEdgeWeight(FullScreenTriangleVSOut input) : SV_Target0
{
  float2 PixelSize;
  NormalTex.GetDimensions(PixelSize.x, PixelSize.y);
  PixelSize = float2(1.0f, 1.0f) / PixelSize;

  float Depth[9];  
  float3 Normal[9];  
  //Retrieve normal and depth data for all neighbors.  
  for (int i=0; i<9; ++i)  
  {  
    float2 uv = input.oTex + offsets[i] * PixelSize;  
    Depth[i] = DepthTex.Sample(PointSampler, uv).r;
    Normal[i]= normalize(NormalTex.Sample(PointSampler, uv).xyz);
  }  
  
  //Compute Deltas in Depth.  
  float4 Deltas1;  
  float4 Deltas2;  
  Deltas1.x = Depth[1];  
  Deltas1.y = Depth[2];  
  Deltas1.z = Depth[3];  
  Deltas1.w = Depth[4];  
  Deltas2.x = Depth[5];  
  Deltas2.y = Depth[6];  
  Deltas2.z = Depth[7];  
  Deltas2.w = Depth[8];  
  //Compute absolute gradients from center.  
  Deltas1 = abs(Deltas1 - Depth[0]);  
  Deltas2 = abs(Depth[0] - Deltas2);  
  //Find min and max gradient, ensuring min != 0  
  float4 maxDeltas = max(Deltas1, Deltas2);  
  float4 minDeltas = max(min(Deltas1, Deltas2), 0.00001);  
  // Compare change in gradients, flagging ones that change  
   // significantly.  
   // How severe the change must be to get flagged is a function of the  
   // minimum gradient. It is not resolution dependent. The constant  
   // number here would change based on how the depth values are stored  
   // and how sensitive the edge detection should be.  
  float4 depthResults = step(minDeltas * 25.0, maxDeltas);  
  //Compute change in the cosine of the angle between normals.  
  Deltas1.x = dot(Normal[1], Normal[0]);  
  Deltas1.y = dot(Normal[2], Normal[0]);  
  Deltas1.z = dot(Normal[3], Normal[0]);  
  Deltas1.w = dot(Normal[4], Normal[0]);  
  Deltas2.x = dot(Normal[5], Normal[0]);  
  Deltas2.y = dot(Normal[6], Normal[0]);  
  Deltas2.z = dot(Normal[7], Normal[0]);  
  Deltas2.w = dot(Normal[8], Normal[0]);  
  Deltas1 = abs(Deltas1 - Deltas2);  
  // Compare change in the cosine of the angles, flagging changes  
   // above some constant threshold. The cosine of the angle is not a  
   // linear function of the angle, so to have the flagging be  
   // independent of the angles involved, an arccos function would be  
   // required.  
  float4 normalResults = step(0.4, Deltas1);  
  normalResults = max(normalResults, depthResults);  

  float w=  (normalResults.x + normalResults.y +  
          normalResults.z + normalResults.w) * 0.25;  

	return float4(w, w, w, 1.0f);
}  


#endif