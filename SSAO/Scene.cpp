#include "DXUT.h"
#include "Scene.h"

Scene::Scene(void)
{
}


Scene::~Scene(void)
{
	for (size_t i = 0; i < mSceneMeshesOpaque.size(); ++i)
	{
		mSceneMeshesOpaque[i].Mesh->Destroy();
		delete mSceneMeshesOpaque[i].Mesh;
	}

	mSceneMeshesOpaque.clear();
}

void Scene::LoadOpaqueMesh( ID3D11Device* d3dDevice, LPCTSTR filename, const D3DXMATRIX& worldMatrix )
{
	SceneMesh entity;
	entity.Mesh = new CDXUTSDKMesh;
	entity.Mesh->Create(d3dDevice, filename);
	entity.World = worldMatrix;
	mSceneMeshesOpaque.push_back(entity);
}
