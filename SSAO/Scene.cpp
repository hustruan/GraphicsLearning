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


	for (size_t i = 0; i < entity.Mesh->GetNumMeshes(); ++i)
	{
		D3DXVECTOR3 center = entity.Mesh->GetMeshBBoxCenter(i);
		D3DXVECTOR3 extent = entity.Mesh->GetMeshBBoxExtents(i);
		
		BoundingBox box(center - extent * 0.5f, center + extent * 0.5f);	
		mWorldBound.Merge(box);
	}
}
