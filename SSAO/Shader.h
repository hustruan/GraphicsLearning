#pragma once

#include <d3d11.h>
#include <d3dx11.h>

template<typename T>
class Shader
{
public:
	Shader(ID3D11Device* d3dDevice, LPCTSTR srcFile, LPCSTR functionName, CONST D3D10_SHADER_MACRO *defines = 0);
	~Shader(void);

	 inline T* GetShader() const { return mShader; }

private:
	// Not implemented
	Shader(const Shader&);
	Shader& operator=(const Shader&);

private:
	T* mShader;
};

ID3D11InputLayout* CreateVertexLayout(ID3D11Device* d3dDevice, const D3D11_INPUT_ELEMENT_DESC* layout, int size, LPCTSTR srcFile, LPCSTR functionName, CONST D3D10_SHADER_MACRO *defines = 0);


typedef Shader<ID3D11VertexShader> VertexShader;
typedef Shader<ID3D11PixelShader> PixelShader;