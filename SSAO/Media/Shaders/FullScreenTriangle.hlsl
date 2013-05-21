#ifndef FullScreenTriangle_HLSL
#define FullScreenTriangle_HLSL

// Used for rendering fullscreen quad
struct FullScreenTriangleVSOut
{
    float4 oPos         : SV_Position;
    float2 oTex         : TEXCOORD0;
};


FullScreenTriangleVSOut FullScreenTriangleVS(uint vertexID : SV_VertexID )
{
    FullScreenTriangleVSOut output;

    // Parametrically work out vertex location for full screen triangle
    float2 grid = float2((vertexID << 1) & 2, vertexID & 2);
    
	output.oPos = float4(grid * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.oTex = grid;
    
    return output;
}


#endif
