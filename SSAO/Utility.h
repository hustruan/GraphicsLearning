#ifndef Utility_h__
#define Utility_h__

#include <d3dx9math.h>

// Returns bounding box [min.xy, max.xy] in clip [-1, 1] space.
D3DXVECTOR4 CalculateLightBound(const D3DXVECTOR3& lightPosView, float lightRadius, float CamearaNear, 
	                            float cameraScaleX, float cameraScaleY);


#endif // Utility_h__
