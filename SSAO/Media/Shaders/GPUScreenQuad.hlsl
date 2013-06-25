#ifndef GPUScreenQuad_HLSL
#define GPUScreenQuad_HLSL

#include "Utility.hlsl"

cbuffer PerFrameConstant : register(b0)
{
	float4x4 Projection;
	float4x4 InvProj;
	float4   CameraNearFar;
};

cbuffer Light : register(b1)
{
	float3 LightColor;
	float3 LightPosition;        // View space light position
	float3 LightDirection; 
	float3 SpotFalloff;
	float2 LightAttenuation;        // begin and end
};

void UpdateClipRegionRoot( float nc, /* Tangent plane x/y normal coordinate (view space) */ 
	                       float lc, /* Light x/y coordinate (view space) */ 
						   float lz, /* Light z coordinate (view space) */ 
						   float lightRadius, float cameraScale, /* Project scale for coordinate (_11 or _22 for x/y respectively) */ 
						   inout float clipMin, inout float clipMax )
{
	float nz = (lightRadius - nc * lc) / lz;
	//float pz = (lc * lc + lz * lz - lightRadius * lightRadius) / (lz - (nz / nc) * lc);
	float pz = lz - lightRadius * nz;

	if (pz > 0.0f) {
		float c = -nz * cameraScale / nc;
		if (nc > 0.0f) {        // Left side boundary
			clipMin = max(clipMin, c);
		} else {                          // Right side boundary
			clipMax = min(clipMax, c);
		}
	}
}

void UpdateClipRegion( float lc, /* Light x/y coordinate (view space) */ 
	                   float lz, /* Light z coordinate (view space) */
					   float lightRadius, float cameraScale, /* Project scale for coordinate (_11 or _22 for x/y respectively) */ 
					   inout float clipMin, inout float clipMax )
{
	float rSq = lightRadius * lightRadius;
	float lcSqPluslzSq = lc * lc + lz * lz;
	float d = rSq * lc * lc - lcSqPluslzSq * (rSq - lz * lz);

	if (d > 0) {
		float a = lightRadius * lc;
		float b = sqrt(d);
		float nx0 = (a + b) / lcSqPluslzSq;
		float nx1 = (a - b) / lcSqPluslzSq;

		UpdateClipRegionRoot(nx0, lc, lz, lightRadius, cameraScale, clipMin, clipMax);
		UpdateClipRegionRoot(nx1, lc, lz, lightRadius, cameraScale, clipMin, clipMax);
	}
}

float4 CalculateLightBound( float3 lightPosView, float lightRadius, float CamearaNear, float cameraScaleX, float cameraScaleY )
{
	// Early out with empty rectangle if the light is too far behind the view frustum
	float4 clipRegion = float4(1, 1, 0, 0);
	if (lightPosView.z + lightRadius >= CamearaNear) {
		float2 clipMin = float2(-1.0f, -1.0f);
		float2 clipMax = float2( 1.0f,  1.0f);

		UpdateClipRegion(lightPosView.x, lightPosView.z, lightRadius, cameraScaleX, clipMin.x, clipMax.x);
		UpdateClipRegion(lightPosView.y, lightPosView.z, lightRadius, cameraScaleY, clipMin.y, clipMax.y);

		clipRegion = float4(clipMin.x, clipMin.y, clipMax.x, clipMax.y);
	}

	return clipRegion;
}


// One per quad - gets expanded in the geometry shader
struct GPUQuadVSOut
{
    float4 QuadCoord : TEXCOORD0;         // [min.xy, max.xy] in clip space
    float  QuadZ     : TEXCOORD1;
};


GPUQuadVSOut GPUQuadVS(in uint lightIndex : SV_VertexID)
{
	GPUQuadVSOut output;

	float lightRadius = LightAttenuation.y;
	
	output.QuadCoord = CalculateLightBound(LightPosition, lightRadius, CameraNearFar.x, Projection._11, Projection._22);
	
	float quadDepth = max(CameraNearFar.x, LightPosition.z - lightRadius);
	float4 quadClip = mul(float4(0.0f, 0.0f, quadDepth, 1.0f), Projection);

    output.QuadZ = quadClip.z / quadClip.w;

	return output;
}

struct GPUQuadGSOut
{
	float3 oTex     : TEXCOORD0;
	float3 oViewRay : TEXCOORD1;
	float4 oPosCS   : SV_Position;
};


[maxvertexcount(4)]
void GPUQuadGS(point GPUQuadVSOut input[1], inout TriangleStream<GPUQuadGSOut> quadStream)
{
	GPUQuadGSOut output;
   
    output.oPosCS.zw = float2(input[0].QuadZ, 1.0f);

	if(all(input[0].QuadCoord.xy < input[0].QuadCoord.zw))
	{
		float4 posVS;

		output.oPosCS.xy = input[0].QuadCoord.xw; // [-1,  1]
		output.oTex = float3(output.oPosCS.xy, output.oPosCS.w);
		
		posVS = mul(output.oPosCS, InvProj);
	    output.oViewRay = float3(posVS.xy / posVS.z, 1.0f);       // Proj to Z=1 plane

		quadStream.Append(output);

		output.oPosCS.xy = input[0].QuadCoord.zw; // [ 1,  1]
		output.oTex = float3(output.oPosCS.xy, output.oPosCS.w);

		posVS = mul(output.oPosCS, InvProj);
	    output.oViewRay = float3(posVS.xy / posVS.z, 1.0f);

		quadStream.Append(output);

		output.oPosCS.xy = input[0].QuadCoord.xy; // [-1, -1]
		output.oTex = float3(output.oPosCS.xy, output.oPosCS.w);

		posVS = mul(output.oPosCS, InvProj);
	    output.oViewRay = float3(posVS.xy / posVS.z, 1.0f);

		quadStream.Append(output);

		output.oPosCS.xy = input[0].QuadCoord.zy; // [ 1, -1]
		output.oTex = float3(output.oPosCS.xy, output.oPosCS.w);

		posVS = mul(output.oPosCS, InvProj);
	    output.oViewRay = float3(posVS.xy / posVS.z, 1.0f);

		quadStream.Append(output);
		
		quadStream.RestartStrip();
	}
}


#endif