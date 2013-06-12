#include "DXUT.h"
#include "DXUTcamera.h"
#include "LightAnimation.h"
#include <fstream>

const float MaxRadius = 100.0f;
const float AttenuationStartFactor = 0.8f;

// Use a constant seed for consistency
std::mt19937 rng(1337);

std::uniform_real<float> RadiusNormDist(0.0f, 1.0f);
std::uniform_real<float> AngleDist(0.0f, 360.0f); 
std::uniform_real<float> HeightDist(0.0f, 20.0f);
std::uniform_real<float> AnimationSpeedDist(2.0f, 20.0f);
std::uniform_int<int> AnimationDirection(0, 1);
std::uniform_real<float> HueDist(0.0f, 1.0f);
std::uniform_real<float> IntensityDist(0.1f, 0.5f);
std::uniform_real<float> AttenuationDist(2.0f, 15.0f);
std::uniform_real<float> SpotInnerAngleDist(30.0f, 45.0f);
std::uniform_real<float> SpotOuterAngleDist(30.0f, 45.0f);
std::uniform_real<float> SpotDropoffDist(0.0f, 50.0f);

namespace {

D3DXVECTOR3 HueToRGB(float hue)
{
	float intPart;
	float fracPart = modf(hue * 6.0f, &intPart);
	int region = static_cast<int>(intPart);

	switch (region) {
	case 0: return D3DXVECTOR3(1.0f, fracPart, 0.0f);
	case 1: return D3DXVECTOR3(1.0f - fracPart, 1.0f, 0.0f);
	case 2: return D3DXVECTOR3(0.0f, 1.0f, fracPart);
	case 3: return D3DXVECTOR3(0.0f, 1.0f - fracPart, 1.0f);
	case 4: return D3DXVECTOR3(fracPart, 0.0f, 1.0f);
	case 5: return D3DXVECTOR3(1.0f, 0.0f, 1.0f - fracPart);
	};

	return D3DXVECTOR3(0.0f, 0.0f, 0.0f);
}

}


LightAnimation::LightAnimation()
	: mTotalTime(0.0f)
{

}

LightAnimation::~LightAnimation()
{

}

void LightAnimation::Move( float elapsedTime )
{
	mTotalTime += elapsedTime;

	// Update positions of active lights
	for (unsigned int i = 0; i < mLights.size(); ++i) 
	{
		// Only animate point light
		if (mLights[i].LightType == LT_PointLight)
		{
			float angle = mLights[i].Angle + mTotalTime * mLights[i].AnimationSpeed;

			mLights[i].LightPosition = D3DXVECTOR3(
				mLights[i].Radius * cosf(angle),
				mLights[i].Height,
				mLights[i].Radius * sinf(angle));
		}		
	}
}

void LightAnimation::RandonPointLight( int numLight )
{
	for (int i = 0; i < numLight; ++i)
	{
		Light light;

		light.LightType = LT_PointLight;

		light.LightColor = IntensityDist(rng) * HueToRGB(HueDist(rng));
		light.LightFalloff.y = AttenuationDist(rng);
		light.LightFalloff.x = AttenuationStartFactor * light.LightFalloff.y;
		
		light.Radius = std::sqrt(RadiusNormDist(rng)) * MaxRadius;
		light.Angle = AngleDist(rng) * D3DX_PI / 180.0f;
		light.Height = HeightDist(rng);
		// Normalize by arc length
		light.AnimationSpeed = (AnimationDirection(rng) * 2 - 1) * AnimationSpeedDist(rng) / light.Radius;

		mLights.push_back(light);
	}

	Move(0);
}

void LightAnimation::RandonSpotLight( int numLight )
{
	for (int i = 0; i < numLight; ++i)
	{
		Light light;

		light.LightType = LT_PointLight;

		light.LightColor = IntensityDist(rng) * HueToRGB(HueDist(rng));
		light.LightFalloff.y = AttenuationDist(rng);
		light.LightFalloff.x = AttenuationStartFactor * light.LightFalloff.y;
		light.SpotFalloff.x =  SpotInnerAngleDist(rng) * D3DX_PI / 180.0f;
		light.SpotFalloff.y =  light.SpotFalloff.x  + SpotOuterAngleDist(rng) * D3DX_PI / 180.0f;
		light.SpotFalloff = D3DXVECTOR3(cosf(light.SpotFalloff.x), cosf(light.SpotFalloff.y), SpotDropoffDist(rng));

		float radius = std::sqrt(RadiusNormDist(rng)) * MaxRadius;
		float angle = AngleDist(rng) * D3DX_PI / 180.0f;
		float height = HeightDist(rng);

		light.LightPosition = D3DXVECTOR3(
			radius * std::cos(angle),
			height,
			radius * std::sin(angle));

		D3DXVec3Normalize(&light.LightDirection, &light.LightPosition);

		mLights.push_back(light);
	}

	Move(0);
}

void LightAnimation::RecordLight( const CFirstPersonCamera& camera, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch(uMsg)
	{
	case WM_KEYDOWN:
		{
			if (wParam == '1' + LT_DirectionalLigt)
			{
				Light light;
				
				D3DXVECTOR3 cameraDir = *camera.GetLookAtPt() - *camera.GetEyePt();
				D3DXVec3Normalize(&cameraDir, &cameraDir);

				light.LightType = LT_DirectionalLigt;
				light.LightDirection = cameraDir;

				mLights.push_back(light);
			}
			else if (wParam == '1' + LT_PointLight)
			{
				Light light;
				
				light.LightType = LT_PointLight;

				light.LightColor = IntensityDist(rng) * HueToRGB(HueDist(rng));
				light.LightFalloff.y = AttenuationDist(rng);
				light.LightFalloff.x = AttenuationStartFactor * light.LightFalloff.y;

				const D3DXVECTOR3& cameraPos = *camera.GetEyePt();
				light.Height = cameraPos.y;
				light.Radius = (std::min)(sqrtf(cameraPos.x*cameraPos.x+cameraPos.z*cameraPos.z), 100.0f);
				light.Angle = atan2f(cameraPos.z, cameraPos.x);
				if (light.Angle < 0) light.Angle += 2.0f * D3DX_PI;
				
				// Normalize by arc length
				light.AnimationSpeed = (AnimationDirection(rng) * 2 - 1) * AnimationSpeedDist(rng) / light.Radius;
				
				mLights.push_back(light);

				Move(0);
			}
			else if (wParam == '1' + LT_SpotLight)
			{
				Light light;

				D3DXVECTOR3 cameraDir = *camera.GetLookAtPt() - *camera.GetEyePt();
				D3DXVec3Normalize(&cameraDir, &cameraDir);

				light.LightType = LT_PointLight;

				light.LightColor = IntensityDist(rng) * HueToRGB(HueDist(rng));
				light.LightPosition = *camera.GetEyePt();
				light.LightDirection = cameraDir;
				light.LightFalloff.y = AttenuationDist(rng);
				light.LightFalloff.x = AttenuationStartFactor * light.LightFalloff.y;
				light.SpotFalloff.x =  SpotInnerAngleDist(rng) * D3DX_PI / 180.0f;
				light.SpotFalloff.y =  light.SpotFalloff.x  + SpotOuterAngleDist(rng) * D3DX_PI / 180.0f;
				light.SpotFalloff = D3DXVECTOR3(cosf(light.SpotFalloff.x), cosf(light.SpotFalloff.y), SpotDropoffDist(rng));

				mLights.push_back(light);

				Move(0);
			}
		}
		break;
	}
}

void LightAnimation::SaveLights( )
{
#define OutputVector2(vec2) "(" << vec2.x << " " << vec2.y << ") "
#define OutputVector3(vec3) "(" << vec3.x << " " << vec3.y << " " << vec3.z << ") "

	if (mLights.empty())
		return;

	std::ofstream stream(mFilename);

	stream << mLights.size() << std::endl;

	for (size_t i = 0; i < mLights.size(); ++i)
	{
		const Light& light = mLights[i];
		switch(light.LightType)
		{
		case LT_DirectionalLigt:
			stream << LT_DirectionalLigt << " " << OutputVector3(light.LightColor) << OutputVector3(light.LightDirection) << std::endl;
			break;
		case LT_PointLight:
			stream << LT_PointLight << " " <<  OutputVector3(light.LightColor) 
				   <<  "(" << light.Radius << " " << light.Angle << " " << light.Height << " " << light.AnimationSpeed << ") "
				   << OutputVector2(light.LightFalloff) << std::endl;
			break;
		case LT_SpotLight:
			stream << LT_SpotLight << " " <<  OutputVector3(light.LightColor) << OutputVector3(light.LightPosition) 
				   << OutputVector3(light.LightDirection) << OutputVector3(light.SpotFalloff) 
				   << OutputVector2(light.LightFalloff) << std::endl;
			break;
		}
	}

#undef OutputVector2
#undef OutputVector3
}

void LightAnimation::LoadLights( const std::string& filename )
{
#define ReadVector2(vec2) dummy >> vec2.x >>  vec2.y >> dummy
#define ReadVector3(vec3) dummy >> vec3.x >>  vec3.y >> vec3.z >> dummy

	char dummy;
	size_t numLights,lightType;
	
	std::ifstream stream(filename);

	mFilename = filename;
	stream >> numLights;

	if (stream.eof())
		return;

	mLights.reserve(numLights);
	for (size_t i = 0; i < numLights; ++i)
	{
		stream >> lightType; 
		
		Light light;
		light.LightType = LightType(lightType);
		switch(light.LightType)
		{
		case LT_DirectionalLigt:
			stream >> ReadVector3(light.LightColor) >> ReadVector3(light.LightDirection);
			break;
		case LT_PointLight:
			stream >> ReadVector3(light.LightColor) 
				   >> dummy >> light.Radius >> light.Angle >> light.Height >> light.AnimationSpeed >> dummy
				   >> ReadVector2(light.LightFalloff);
			break;
		case LT_SpotLight:
			stream >> ReadVector3(light.LightColor) >> ReadVector3(light.LightPosition) >> ReadVector3(light.LightDirection)
				   >> ReadVector3(light.SpotFalloff) >> ReadVector2(light.LightFalloff);
			break;
		}
		mLights.push_back(light);
	}


	Move(0);

#undef ReadVector2
#undef ReadVector3
}




