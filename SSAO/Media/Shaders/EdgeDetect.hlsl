#ifndef EdgeDetect_HLSL
#define EdgeDetect_HLSL

Texture2D NormalTex    : register(t0);   
Texture2D DepthTex     : register(t1);
Texture2D ColorTex     : register(t1);

SamplerState PointSampler : register(s0);   

//const static float2 TexOffset[] =  
//{
//	float2(-1.0f, -1.0f), // Top Left
//	float2( 1.0f,  1.0f), // Bottom Right
//	float2(-1.0f,  1.0f), // Top Right
//	float2( 1.0f, -1.0f), // Bottom Left
//	float2(-1.0f,  0.0f), // Left    
//    float2( 0.0f, -1.0f)  // Top  
//};

cbuffer EdgeDetectCB
{
	float2 e_barrier = float2(0.8f,0.00001f);  // x=norm, y=depth
	float2 e_weights= float2(1,1);             // x=norm, y=depth
	float2 e_kernel = float2(1,1);             // x=norm, y=depth
};


const static int2 TexOffset[7] =  
{
	int2( 0,  0), // Center
	int2(-1, -1), // Top Left
	int2( 1,  1), // Bottom Right
	int2(-1,  1), // Top Right
	int2( 1, -1), // Bottom Left
	int2(-1,  0), // Left    
    int2( 0, -1)  // Top  
};



float4 EdgeDetectPS(in screenPos : SV_Position, 
                    in float2 iTex : TEXCOORD0) : SV_Target0
{
	// Normal discontinuity filter
	int2 texelPos = int2(screenPos.xy);
		
	float3 nc = NormalTex.Load(texelPos).xyz;
	nc = normalize(nc);

	float4 nd;	
	nd.x =  dot(nc, normalize(NormalTex.Sample(PointSampler, texelPos + TexOffset[1] * PixelSize).xyz));
	nd.y =  dot(nc, normalize(NormalTex.Sample(PointSampler, texelPos + TexOffset[2] * PixelSize).xyz));
	nd.z =  dot(nc, normalize(NormalTex.Sample(PointSampler, texelPos + TexOffset[3] * PixelSize).xyz));
	nd.w =  dot(nc, normalize(NormalTex.Sample(PointSampler, texelPos + TexOffset[4] * PixelSize).xyz));

	nd -= e_barrier.x;
	nd = step(0, nd);
	float ne = saturate(dot(nd, e_weights.x));


	// Depth filter : compute gradiental difference:
	// (c-sample1)+(c-sample1_opposite)

	float dc = DepthTex.Load(texelPos).r;

	float4 dd;

	dd.x = DepthTex.Sample(PointSampler, texelPos + TexOffset[1] * PixelSize).r +
           DepthTex.Sample(PointSampler, texelPos + TexOffset[2] * PixelSize).r;

	dd.y = DepthTex.Sample(PointSampler, texelPos + TexOffset[3] * PixelSize).r +
           DepthTex.Sample(PointSampler, texelPos + TexOffset[4] * PixelSize).r;

	dd.z = DepthTex.Sample(PointSampler, texelPos + TexOffset[5] * PixelSize).r +
           DepthTex.Sample(PointSampler, texelPos - TexOffset[5] * PixelSize).r;

	dd.w = DepthTex.Sample(PointSampler, texelPos + TexOffset[6] * PixelSize).r +
           DepthTex.Sample(PointSampler, texelPos - TexOffset[6] * PixelSize).r;

	dd = abs(2 * dc.z - dd)- e_barrier.y;
	dd = step(dd, 0);

	float de = saturate(dot(dd, e_weights.y));

	// Weight
    float w = (1 - de * ne) * e_kernel.x; // 0 - no aa, 1=full aa

	// Smoothed color
    // (a-c)*w + c = a*w + c(1-w)

    float2 offset = iTex * (1-w);
    half4 s0 = tex2D(s_image, offset + (iTex + TexOffset[1] * PixelSize) * w);
    half4 s1 = tex2D(s_image, offset + (iTex + TexOffset[2] * PixelSize) * w);
    half4 s2 = tex2D(s_image, offset + (iTex + TexOffset[3] * PixelSize) * w);
    half4 s3 = tex2D(s_image, offset + (iTex + TexOffset[4] * PixelSize) * w);

    return (s0 + s1 + s2 + s3) / 4.0f;
}



#endif