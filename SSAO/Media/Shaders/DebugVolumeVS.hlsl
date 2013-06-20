cbuffer PerOjectConstant
{
	float4x4 WorldView;
	float4x4 WorldViewProj;
};

void DebugPointLightVS(in float3 iPos : POSITION, out float4 oPos : SV_Position) 
{
	oPos = mul(float4(iPos, 1.0f), WorldViewProj);
}

