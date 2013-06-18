#include "DXUT.h"
#include "Utility.h"
#include <algorithm>

void UpdateClipRegionRoot( float nc, /* Tangent plane x/y normal coordinate (view space) */ 
	                       float lc, /* Light x/y coordinate (view space) */ 
						   float lz, /* Light z coordinate (view space) */ 
						   float lightRadius, float cameraScale, /* Project scale for coordinate (_11 or _22 for x/y respectively) */ 
						   float& clipMin, float& clipMax )
{
	float nz = (lightRadius - nc * lc) / lz;
	//float pz = (lc * lc + lz * lz - lightRadius * lightRadius) / (lz - (nz / nc) * lc);
	float pz = lz - lightRadius * nz;

	if (pz > 0.0f) {
		float c = -nz * cameraScale / nc;
		if (nc > 0.0f) {        // Left side boundary
			clipMin = (std::max)(clipMin, c);
		} else {                          // Right side boundary
			clipMax = (std::min)(clipMax, c);
		}
	}
}

void UpdateClipRegion( float lc, /* Light x/y coordinate (view space) */ 
	                   float lz, /* Light z coordinate (view space) */
					   float lightRadius, float cameraScale, /* Project scale for coordinate (_11 or _22 for x/y respectively) */ 
					   float& clipMin, float& clipMax )
{
	float rSq = lightRadius * lightRadius;
	float lcSqPluslzSq = lc * lc + lz * lz;
	float d = rSq * lc * lc - lcSqPluslzSq * (rSq - lz * lz);

	if (d > 0) {
		float a = lightRadius * lc;
		float b = sqrt(d);
		float nx0 = (a + b) / lcSqPluslzSq;
		float nx1 = (a - b) / lcSqPluslzSq;

		UpdateClipRegionRoot(nx0, lc, lz, lightRadius, cameraScale, clipMin, clipMax);
		UpdateClipRegionRoot(nx1, lc, lz, lightRadius, cameraScale, clipMin, clipMax);
	}
}

D3DXVECTOR4 CalculateLightBound( const D3DXVECTOR3& lightPosView, float lightRadius, float CamearaNear, float cameraScaleX, float cameraScaleY )
{
	// Early out with empty rectangle if the light is too far behind the view frustum
	D3DXVECTOR4 clipRegion = D3DXVECTOR4(1, 1, 0, 0);
	if (lightPosView.z + lightRadius >= CamearaNear) {
		D3DXVECTOR2 clipMin = D3DXVECTOR2(-1.0f, -1.0f);
		D3DXVECTOR2 clipMax = D3DXVECTOR2( 1.0f,  1.0f);

		UpdateClipRegion(lightPosView.x, lightPosView.z, lightRadius, cameraScaleX, clipMin.x, clipMax.x);
		UpdateClipRegion(lightPosView.y, lightPosView.z, lightRadius, cameraScaleY, clipMin.y, clipMax.y);

		clipRegion = D3DXVECTOR4(clipMin.x, clipMin.y, clipMax.x, clipMax.y);
	}

	return clipRegion;
}

