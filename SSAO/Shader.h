#pragma once

#include <d3d11.h>
#include <d3dx11.h>
#include <memory>

template<typename T>
class Shader
{
public:
	typedef T shader_type;

public:
	Shader(T* pShader) : mShader(pShader) {}
	~Shader(void) { SAFE_RELEASE(mShader); }

	 inline T* GetShader() const { return mShader; }

private:
	// Not implemented
	Shader(const Shader&);
	Shader& operator=(const Shader&);

private:
	T* mShader;
};

typedef Shader<ID3D11VertexShader> VertexShader;
typedef Shader<ID3D11PixelShader> PixelShader;
typedef Shader<ID3D11GeometryShader> GeometryShader;

using std::shared_ptr;

struct ShaderFactory
{
	template<typename T>	
	static shared_ptr<T>  CreateShader(ID3D11Device* d3dDevice, LPCTSTR srcFile, LPCSTR functionName, CONST D3D10_SHADER_MACRO *defines = 0)
	{
		shared_ptr<T> retVal;

		ID3DBlob* pBytecode = nullptr;
		HRESULT hr = CompileShaderFromFile(srcFile, functionName, defines, GetShaderProfileString<T::shader_type>(), &pBytecode);

		if (SUCCEEDED(hr))
			retVal = std::make_shared<T>( CreateShaderD3D11<T::shader_type>(d3dDevice, pBytecode->GetBufferPointer(), pBytecode->GetBufferSize()) );

		SAFE_RELEASE(pBytecode);

		return retVal;
	}

	static ID3D11InputLayout* CreateVertexLayout(ID3D11Device* d3dDevice, const D3D11_INPUT_ELEMENT_DESC* layout, int size, LPCTSTR srcFile, LPCSTR functionName, CONST D3D10_SHADER_MACRO *defines = 0);

	static shared_ptr<GeometryShader> CreateGeometryShaderWithStreamOutput(ID3D11Device* d3dDevice, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream, LPCTSTR srcFile, LPCSTR functionName, CONST D3D10_SHADER_MACRO *defines = 0);

private:
	static HRESULT CompileShaderFromFile( LPCTSTR szFileName, LPCSTR szEntryPoint, CONST D3D10_SHADER_MACRO *defines, LPCSTR szShaderModel, ID3DBlob** ppBlobOut );
	
	template <typename T> static LPCSTR GetShaderProfileString();
	template <typename T> static T* CreateShaderD3D11(ID3D11Device* d3dDevice, const void* shaderBytecode, size_t bytecodeLength);

	// Vertex Shader
	template<>
	inline static LPCSTR GetShaderProfileString<ID3D11VertexShader>() { return "vs_4_0"; }

	template<>
	inline static ID3D11VertexShader* CreateShaderD3D11<ID3D11VertexShader>(ID3D11Device* d3dDevice, const void* shaderBytecode, size_t bytecodeLength)
	{
		ID3D11VertexShader *shader = 0;
		d3dDevice->CreateVertexShader(shaderBytecode, bytecodeLength, 0, &shader);
		return shader;
	}

	// Pixel Shader
	template<>
	inline static LPCSTR GetShaderProfileString<ID3D11PixelShader>() { return "ps_4_0"; }

	template<>
	inline static ID3D11PixelShader* CreateShaderD3D11<ID3D11PixelShader>(ID3D11Device* d3dDevice, const void* shaderBytecode, size_t bytecodeLength)
	{
		ID3D11PixelShader *shader = 0;
		d3dDevice->CreatePixelShader(shaderBytecode, bytecodeLength, 0, &shader);
		return shader;
	}

	// Geometry Shader
	template<>
	inline static LPCSTR GetShaderProfileString<ID3D11GeometryShader>() { return "gs_4_0"; }

	template<>
	inline static ID3D11GeometryShader* CreateShaderD3D11<ID3D11GeometryShader>(ID3D11Device* d3dDevice, const void* shaderBytecode, size_t bytecodeLength)
	{
		ID3D11GeometryShader *shader = 0;
		d3dDevice->CreateGeometryShader(shaderBytecode, bytecodeLength, 0, &shader);
		return shader; 
	}
};
