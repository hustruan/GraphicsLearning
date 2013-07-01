#ifndef ShaderContanst_h__
#define ShaderContanst_h__

#include <d3dx9math.h>

struct HBAOParams
{
	D3DXVECTOR2 AOResolution;
	D3DXVECTOR2 InvAOResolution;

	D3DXVECTOR2 FocalLen;
	D3DXVECTOR2 ProjDepthScale; //(M33, M43)

	float Radius;
	float RadiusSquared;
	float InvRadiusSquared;
	float MaxRadiusPixels;

	float TanAngleBias;
	float Strength;

	float pad[2];
};


#endif // ShaderContanst_h__
