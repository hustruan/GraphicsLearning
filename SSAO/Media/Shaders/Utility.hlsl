#ifndef Utility_HLSL
#define Utility_HLSL

/**
 * Normal encoding
 */
float3 EncodeNormal(in float3 normal)
{
	return normal * 0.5f + 0.5f;
}

float3 DecodeNormal(in float3 normal)
{
	return normal * 2.0f - 1.0f;
}


/**
 * @brief Convert clip space coord to texture uv coord.
 */
float2 ConvertUV(in float2 posCS)
{
	// Convert [-1, 1]x[1, -1] to [0, 1]x[0, 1]
	return 0.5f * float2(posCS.x, -posCS.y) + float2(0.5f, 0.5f);
}

/**
 * @brief Convert non-linear ZBuffer depth to linear view space depth
 *
 * @Param nonLinearDepth	depth from non-linear ZBuffer
 */
float LinearizeDepth(in float nonLinearDepth, in float nearPlane, in float farPlane)
{	
	return nearPlane * farPlane / (farPlane - (farPlane - nearPlane) * nonLinearDepth);
}


/**
 * @param lPosView    view space position of light volume mesh vertices
 *
 * @param linearDepth    depth in view space
 */
float3 PositionVSFromDepth(in float3 lPosView, in float linearDepth)
{
    // Clamp the view space position to the plane at Z = 1
	float3 viewRay = float3(lPosView.xy / lPosView.z, 1.0f);
	return viewRay * linearDepth;
}

/**
 * @brief Calculate the Fresnel factor using Schlick's approximation
 */
float3 CalculateFresnel(in float3 specAlbedo, in float3 L, in float3 H) 
{
    float3 fresnel = specAlbedo;
    fresnel += (1.0f - specAlbedo) * pow((1.0f - saturate(dot(L, H))), 5.0f);
    return saturate(fresnel);
}

/**
 * @brief Calculate an approximate Fresnel factor using N dot V instead of L dot H
 *        Which is used for environment map fresnel.
 */
float3 CalculateAmbiemtFresnel(in float3 specAlbedo, in float3 N, in float3 V) 
{
	float3 fresnel = specAlbedo;
    fresnel += (1.0f - specAlbedo) * pow((1.0f - saturate(dot(N, V))), 5.0f);
    return saturate(fresnel);
}

/**
 * @brief Computes the specular contribution of a light using normalized Blinn-Phong
 */
float CalculateSpecular(in float3 N, in float3 H, in float specPower)
{
	float nDotH = saturate(dot(N, H));
    return pow(nDotH, specPower) * ((specPower + 2.0f) / 8.0f);
}

/**
 */
float CalculateAttenuation(in float dist, in float attenBegin, in float attenEnd)
{
	return saturate( (attenEnd-dist)/(attenEnd-attenBegin) );
}

float CalculateSpotCutoff(in float cosAngle, in float innerCos, in float outerCos, in float power)
{
	return pow( saturate((cosAngle-outerCos)/(innerCos-outerCos)), power );
}

//void CalculateBlinPhongBRDF(float3 normal, float3 lightDir, float3 viewDir, float3 lightIntensity, float specularPower, 
//                    inout float3 litDiffuse, inout float3 litSpecular)
//{
//	float NdotL = dot(normal, lightDir);

//	if(NdotL > 0.0f)
//	{
//		float3 h = reflect(lightDir, viewDir);
//		float RdotV = max(0.0f, dot(r, viewDir));

//		litDiffuse += lightIntensity * NdotL;
//		litSpecular += lightIntensity * pow(RdotV, specularPower);;
//	}
//}

#endif