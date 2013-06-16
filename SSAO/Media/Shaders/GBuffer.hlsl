#ifndef GBuffer_HLSL
#define GBuffer_HLSL

#include "PerFrameConstants.hlsl"
#include "Utility.hlsl"

struct GBuffer
{
	float4 Normal : SV_Target0;     // Normal + shininess
    float4 Albedo : SV_Target1;     // Diffuse + Specular
};

Texture2D BestFitTexture : register(t0); 
SamplerState NormalSampler : register(s0);

Texture2D DiffuseTexture : register(t1);
SamplerState DiffuseSampler : register(s1);

struct GeometryVSIn
{
    float3 iPos       : POSITION;
    float3 iNormal    : NORMAL;
    float2 iTex       : TEXCOORD0;
};

struct GeometryVSOut
{
    float4 oPos       : SV_Position;
    float3 oNormal    : TEXCOORD0;
    float2 oTex       : TEXCOORD1;
};

void CompressUnsignedNormalToNormalsBuffer(inout float3 vNormal)
{
  // renormalize (needed if any blending or interpolation happened before)
  vNormal.rgb = normalize(vNormal.rgb);
  // get unsigned normal for cubemap lookup (note the full float precision is required)
  float3 vNormalUns = abs(vNormal.rgb);
  // get the main axis for cubemap lookup
  float maxNAbs = max(vNormalUns.z, max(vNormalUns.x, vNormalUns.y));
  // get texture coordinates in a collapsed cubemap
  float2 vTexCoord = vNormalUns.z<maxNAbs?(vNormalUns.y<maxNAbs?vNormalUns.yz:vNormalUns.xz):vNormalUns.xy;
  vTexCoord = vTexCoord.x < vTexCoord.y ? vTexCoord.yx : vTexCoord.xy;
  vTexCoord.y /= vTexCoord.x;
  // fit normal into the edge of unit cube
  vNormal.rgb /= maxNAbs;
  // look-up fitting length and scale the normal to get the best fit
  float fFittingScale = BestFitTexture.Sample(NormalSampler, vTexCoord).a;
  // scale the normal to get the best fit
  vNormal.rgb *= fFittingScale;
  // squeeze back to unsigned
  vNormal.rgb = vNormal.rgb * .5f + .5f;
}

GeometryVSOut GeometryVS(GeometryVSIn input)
{
    GeometryVSOut output;

    output.oPos     = mul(float4(input.iPos, 1.0f), WorldViewProj);
    output.oNormal  = mul(float4(input.iNormal, 0.0f), WorldView).xyz;
    output.oTex     = input.iTex;
    
    return output;
}

void GeometryPS(GeometryVSOut input, out GBuffer outputGBuffer)
{
    float4 albedo = DiffuseTexture.Sample(DiffuseSampler, input.oTex);
    float3 normal = input.oNormal;
     
	const float Shininess = 80.0f;
    const float Specular = 0.5f;

	CompressUnsignedNormalToNormalsBuffer(normal);

    outputGBuffer.Normal = float4(normal, Shininess/256.0f);
    outputGBuffer.Albedo = float4(albedo.rgb, Specular);
}

#endif // GBuffer_HLSL
