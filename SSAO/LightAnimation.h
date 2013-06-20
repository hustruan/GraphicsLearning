#ifndef LightAnimation_h__
#define LightAnimation_h__

#include <vector>
#include <cmath>
#include <random>

class CFirstPersonCamera;

enum LightType 
{
	LT_DirectionalLigt = 0,
	LT_PointLight,
	LT_SpotLight,
};

class LightAnimation
{
public:
	struct Light
	{
		LightType   LightType;
		D3DXVECTOR3 LightColor;
		D3DXVECTOR3 LightPosition;
		D3DXVECTOR3 LightDirection;
		D3DXVECTOR3 SpotFalloff;
		D3DXVECTOR2 LightAttenuation;

		// For animation
		float Radius;
		float Angle;
		float Height;
		float AnimationSpeed;

		//Light() {}

		//// Directional
		//Light(const D3DXVECTOR3& color, const D3DXVECTOR3& direction)
		//	: LightType(LT_DirectionalLigt), LightColor(color), LightDirection(direction) {}

		//// Point
		//Light(const D3DXVECTOR3& color, const D3DXVECTOR3& position, float attenuationBegin, float attenuationEnd)
		//	: LightType(LT_PointLight), LightColor(color), LightPosition(position), LightAttenuation(attenuationBegin, attenuationEnd) {}
	
		//// Spot
		//Light(const D3DXVECTOR3& color, const D3DXVECTOR3& position, const D3DXVECTOR3& direction, 
		//	float innerAngle, float outAngle, float spotFalloff, float attenuationBegin, float attenuationEnd)
		//	: LightType(LT_SpotLight), LightColor(color), LightPosition(position), LightDirection(direction),
		//	  SpotFalloff(cosf(innerAngle), cosf(outAngle), spotFalloff), LightAttenuation(attenuationBegin, attenuationEnd) {}
	};

public:
	LightAnimation();
	~LightAnimation();

	void Move(float elapsedTime);

	void RandonPointLight(int numLight);
	void RandonSpotLight(int numLight);

	void RecordLight(const CFirstPersonCamera& camera, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void SaveLights();
	void LoadLights(const std::string& filename);

public:
	std::vector<Light> mLights;

private:
	std::string mFilename;
	float mTotalTime;
};


#endif // LightAnimation_h__
