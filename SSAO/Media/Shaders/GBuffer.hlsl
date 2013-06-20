#ifndef GBuffer_HLSL
#define GBuffer_HLSL

cbuffer PerOjectConstant : register(b0)
{
	float4x4 WorldView;
	float4x4 WorldViewProj;
};

static const float Shininess = 80.0f;
static const float Specular = 0.5f;

Texture2D BestFitTexture : register(t0); 
Texture2D DiffuseTexture : register(t1);

SamplerState NormalSampler : register(s0);
SamplerState DiffuseSampler : register(s1);

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

void VSMain(in float3 iPos       : POSITION,
            in float3 iNormal    : NORMAL,
            in float2 iTex       : TEXCOORD0,
			out float4 oPosCS    : SV_Position,
			out float3 oNormal   : NORMAL,
			out float2 oTex      : TEXCOORD0)
{
	oPosCS   = mul(float4(iPos, 1.0f), WorldViewProj);
    oNormal  = mul(float4(iNormal, 0.0f), WorldView).xyz;
    oTex     = iTex;
}

struct GBuffer
{
	float4 Normal : SV_Target0;     // Normal + shininess
    float4 Albedo : SV_Target1;     // Diffuse + Specular
};


GBuffer PSMain(in float3 iNormal : NORMAL, in float2 iTex : TEXCOORD0)
{
	GBuffer output;

    float4 albedo = DiffuseTexture.Sample(DiffuseSampler, iTex);

	float3 normal = iNormal;
	CompressUnsignedNormalToNormalsBuffer(normal);

    output.Normal = float4(normal, Shininess/256.0f);
    output.Albedo = float4(albedo.rgb, Specular);

	return output;
}

#endif // GBuffer_HLSL
