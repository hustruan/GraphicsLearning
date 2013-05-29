#ifndef Utility_HLSL
#define Utility_HLSL

/**
 * @brief Convert non-linear ZBuffer depth to linear view space depth
 *
 * @Param nonLinearDepth
 *        depth from non-linear ZBuffer
 */
float LinearizeDepth(float nonLinearDepth, float nearPlane, float farPlane)
{	
	return nearPlane * farPlane / (farPlane - (farPlane - nearPlane) * nonLinearDepth);
}


/**
 * @param lPosView
 *        View space position of light volume mesh vertices
 *
 * @param linearDepth
 */
float3 PositionVSFromDepth(float3 lPosView, float linearDepth)
{
    // Clamp the view space position to the plane at Z = 1
	float3 viewRay = float3(lPosView.xy / lPosView.z, 1.0f);
	return viewRay * linearDepth;
}

#endif