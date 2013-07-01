#pragma once

#include "LightAnimation.h"
#include "SDKmesh.h"
#include "BoundingVolume.h"
#include <vector>

class Scene
{
public:
	Scene(void);
	~Scene(void);

	void LoadOpaqueMesh(ID3D11Device* d3dDevice, LPCTSTR filename, const D3DXMATRIX& worldMatrix);

public:
	LightAnimation mLightAnimation;

	BoundingBox mWorldBound;

	struct SceneMesh
	{
		CDXUTSDKMesh* Mesh;
		D3DXMATRIX World;
	};

	std::vector<SceneMesh> mSceneMeshesOpaque;
	std::vector<SceneMesh*> mSceneMeshesAlpha;
};

