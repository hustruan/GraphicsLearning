#include "DXUT.h"
#include "Shader.h"
#include <exception>


HRESULT ShaderFactory::CompileShaderFromFile( LPCTSTR szFileName, LPCSTR szEntryPoint, CONST D3D10_SHADER_MACRO *defines, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile( szFileName, defines, NULL, szEntryPoint, szShaderModel, 
		dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );

	if( FAILED(hr) )
	{
		if( pErrorBlob != NULL )
			OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
		SAFE_RELEASE( pErrorBlob );

		throw std::exception("Error compiling shader");
	}
	SAFE_RELEASE( pErrorBlob );

	return S_OK;
}

ID3D11InputLayout* ShaderFactory::CreateVertexLayout( ID3D11Device* d3dDevice, const D3D11_INPUT_ELEMENT_DESC* layout, int size, LPCTSTR srcFile, LPCSTR functionName, CONST D3D10_SHADER_MACRO *defines /*= 0*/ )
{
	ID3DBlob* pBytecode = NULL;
	HRESULT hr = CompileShaderFromFile(srcFile, functionName, defines, GetShaderProfileString<ID3D11VertexShader>(), &pBytecode);

	ID3D11InputLayout* retVal = nullptr;

	if (SUCCEEDED(hr))
		d3dDevice->CreateInputLayout(layout, size, pBytecode->GetBufferPointer(), pBytecode->GetBufferSize(), &retVal);

	SAFE_RELEASE( pBytecode );

	return retVal;
}

shared_ptr<GeometryShader> ShaderFactory::CreateGeometryShaderWithStreamOutput( ID3D11Device* d3dDevice, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream, LPCTSTR srcFile, LPCSTR functionName, CONST D3D10_SHADER_MACRO *defines /*= 0*/ )
{
	ID3DBlob* pBytecode = NULL;
	HRESULT hr = CompileShaderFromFile(srcFile, functionName, defines, GetShaderProfileString<ID3D11GeometryShader>(), &pBytecode);

	shared_ptr<GeometryShader> retVal = nullptr;

	if (SUCCEEDED(hr))
	{
		ID3D11GeometryShader *shader = 0;
		hr = d3dDevice->CreateGeometryShaderWithStreamOutput(pBytecode->GetBufferPointer(), pBytecode->GetBufferSize(), pSODeclaration, NumEntries,
			pBufferStrides, NumStrides, RasterizedStream, NULL, &shader);

		retVal = std::make_shared<GeometryShader>(shader);
	}

	return retVal;
}
