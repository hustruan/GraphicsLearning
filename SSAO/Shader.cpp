#include "DXUT.h"
#include "Shader.h"
#include <exception>

namespace ShaderFactory
{
	 template <typename T> LPCSTR GetShaderProfileString();
	 template <typename T> T* CreateShader(ID3D11Device* d3dDevice, const void* shaderBytecode, size_t bytecodeLength);

	 // Vertex Shader
	 template<>
	 inline LPCSTR GetShaderProfileString<ID3D11VertexShader>() { return "vs_4_0"; }

	 template<>
	 inline ID3D11VertexShader* CreateShader<ID3D11VertexShader>(ID3D11Device* d3dDevice, const void* shaderBytecode, size_t bytecodeLength)
	 {
		 ID3D11VertexShader *shader = 0;
		 HRESULT hr = d3dDevice->CreateVertexShader(shaderBytecode, bytecodeLength, 0, &shader);
		 if (FAILED(hr)) {
			 assert(false);
		 }
		 return shader;
	 }

	 // Pixel Shader
	 template<>
	 inline LPCSTR GetShaderProfileString<ID3D11PixelShader>() { return "ps_4_0"; }

	 template<>
	 inline ID3D11PixelShader* CreateShader<ID3D11PixelShader>(ID3D11Device* d3dDevice, const void* shaderBytecode, size_t bytecodeLength)
	 {
		 ID3D11PixelShader *shader = 0;
		 HRESULT hr = d3dDevice->CreatePixelShader(shaderBytecode, bytecodeLength, 0, &shader);
		 if (FAILED(hr)) {
			 assert(false);
		 }
		 return shader;
	 }
}

ID3D11InputLayout* CreateVertexLayout( ID3D11Device* d3dDevice, const D3D11_INPUT_ELEMENT_DESC* layout, int size, LPCTSTR srcFile, LPCSTR szEntryPoint, CONST D3D10_SHADER_MACRO *defines /*= 0*/ )
{
	HRESULT hr; 

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

	ID3D11InputLayout* pVertexLayout = 0;

	ID3D10Blob* pBytecode = 0;
	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile( srcFile, defines, NULL, szEntryPoint, "vs_4_0", 
		dwShaderFlags, 0, NULL, &pBytecode, &pErrorBlob, NULL );

	assert(SUCCEEDED(hr));

	hr = d3dDevice->CreateInputLayout(layout, size, pBytecode->GetBufferPointer(), 
		pBytecode->GetBufferSize(), &pVertexLayout);
	assert(SUCCEEDED(hr));

	SAFE_RELEASE( pErrorBlob );
	SAFE_RELEASE( pBytecode );
	return pVertexLayout;
}


template<typename T>
Shader<T>::Shader( ID3D11Device* d3dDevice, LPCTSTR srcFile, LPCSTR szEntryPoint, CONST D3D10_SHADER_MACRO *defines /*= 0*/ )
{
	HRESULT hr; 
	
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif
	
	ID3D10Blob* pBytecode = 0;
	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile( srcFile, defines, NULL, szEntryPoint, ShaderFactory::GetShaderProfileString<T>(), 
		dwShaderFlags, 0, NULL, &pBytecode, &pErrorBlob, NULL );

	if( FAILED(hr) )
	{
		if( pErrorBlob != NULL )
			OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
		SAFE_RELEASE( pErrorBlob );

		 throw std::exception("Error compiling shader");
	}
	SAFE_RELEASE( pErrorBlob );

	mShader = ShaderFactory::CreateShader<T>(d3dDevice, pBytecode->GetBufferPointer(), pBytecode->GetBufferSize());
	SAFE_RELEASE( pBytecode );
}

template<typename T>
Shader<T>::~Shader( void )
{
	SAFE_RELEASE( mShader );
}


// Explicit template instantiation
template class Shader<ID3D11VertexShader>;
template class Shader<ID3D11PixelShader>;