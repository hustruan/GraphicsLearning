#ifndef GBuffer_HLSL
#define GBuffer_HLSL

struct GBuffer
{
	float4 Normal : SV_Target0;     // Normal + shininess
    float4 Albedo : SV_Target1;     // Diffuse + Specular
};

float3 EncodeNormal(float3 normal)
{
	return normal * 0.5 + 0.5;
}

float3 DecodeNormal(float3 normal)
{
	return normal * 2.0 - 1.0;;
}




#endif // GBuffer_HLSL
