cbuffer Light
{
	float3 LightColor;
	float3 LightPosition;        // View space light position
	float3 LightDirection; 
	float3 SpotFalloff;
	float2 LightAttenuation;   // begin and end
};

float4 DebugPointLightPS() : SV_Target0
{
	return float4(LightColor, 1.0f);
}