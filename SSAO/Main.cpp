//--------------------------------------------------------------------------------------
// File: SSAO.cpp
//
// Empty starting point for new Direct3D 9 and/or Direct3D 11 applications
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"

#include "SSAO.h"
#include "Scene.h"
#include "LightAnimation.h"

#include <sstream>

CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CDXUTDialog                 g_HUD;                  // manages the 3D   
CD3DSettingsDlg             g_D3DSettingsDlg;       // Device settings dialog
CDXUTComboBox*              g_SceneSelectCombo;
CDXUTComboBox*              g_ShadingSelectCombo;
CDXUTTextHelper*            g_TextHelper;
CFirstPersonCamera          g_Camera;               // A FPS camera

SSAO*                       g_SSAO;
Scene*                      g_Scene;
LightAnimation*             g_LightAnimation;


enum SceneSelection
{
	Scene_Power_Plant,
	Scene_Sponza,
};

enum ShadingSelection
{
	Shading_Forward,
	Shading_DeferredShading,
};


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_SCENE_SELECTION     5
#define IDC_SHADING_SELECTION   6

void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

void InitUI();
void InitScene(ID3D11Device* d3dDevice);
void DestroyScene();

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
	InitUI();

}

void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
	switch( nControlID )
	{
	case IDC_TOGGLEFULLSCREEN:
		DXUTToggleFullScreen(); break;
	case IDC_TOGGLEREF:
		DXUTToggleREF(); break;
	case IDC_CHANGEDEVICE:
		g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
	case IDC_SCENE_SELECTION:
		DestroyScene(); break;
	}
}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
	HRESULT hr;

	ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
	V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
	V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );

	g_TextHelper = new CDXUTTextHelper(pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15);

	// init SSAO Render
	if (!g_SSAO)
		g_SSAO = new SSAO(pd3dDevice);

	g_Camera.SetRotateButtons(true, false, false);
	g_Camera.SetDrag(true);
	g_Camera.SetEnableYAxisMovement(true);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
	HRESULT hr;

	V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
	V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );


	// Setup the camera's projection parameters
	float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
	
	g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.5f, 300.0f);

	// Standard HUDs
	const int border = 20;
	g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - g_HUD.GetWidth() - border, 0 );

	if (g_SSAO)
		g_SSAO->OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc);
	
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	g_Camera.FrameMove(fElapsedTime);

	//if (g_LightAnimation)
		//g_LightAnimation->Move(fElapsedTime);
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
	// If the settings dialog is being shown, then render it instead of rendering the app's scene
	if( g_D3DSettingsDlg.IsActive() )
	{
		g_D3DSettingsDlg.OnRender( fElapsedTime );
		return;
	}

	// Lazily load scene
	if (!g_Scene) 
	{
		InitScene(pd3dDevice);
	}

    // Clear render target and the depth stencil 
    float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };

    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

	D3D11_VIEWPORT viewport;
	viewport.Width    = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Width);
	viewport.Height   = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	ShadingSelection shadingType = static_cast<ShadingSelection>(PtrToUlong(g_ShadingSelectCombo->GetSelectedData()));
	switch(shadingType)
	{
	case Shading_Forward:
		g_SSAO->RenderForward(pd3dImmediateContext, pRTV, pDSV, *g_Scene, *g_LightAnimation, g_Camera, &viewport);
		break;
	case Shading_DeferredShading:
		g_SSAO->Render(pd3dImmediateContext, pRTV, pDSV, *g_Scene, *g_LightAnimation, g_Camera, &viewport);
		break;
	}

	// reset render target
	pd3dImmediateContext->RSSetViewports(1, &viewport);
	pd3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDSV);

	g_HUD.OnRender( fElapsedTime );

	// Render text
	g_TextHelper->Begin();

	g_TextHelper->SetInsertionPos(2, 0);
	g_TextHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 0.0f, 1.0f));
	g_TextHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
	g_TextHelper->DrawTextLine(DXUTGetDeviceStats());

	// Output frame time
	{
		std::wostringstream oss;
		D3DXVECTOR3 eye = *g_Camera.GetEyePt();
		D3DXVECTOR3 dir = *g_Camera.GetLookAtPt();
		D3DXVec3Normalize(&dir, &dir);
		oss << 1000.0f / DXUTGetFPS() << " ms / frame" << " Camera Pos: (" << eye.x << " " << eye.y << " " << eye.z << ") ";
		g_TextHelper->DrawTextLine(oss.str().c_str());
	}

	g_TextHelper->End();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
	 g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
	g_DialogResourceManager.OnD3D11DestroyDevice();
	g_D3DSettingsDlg.OnD3D11DestroyDevice();
	DXUTGetGlobalResourceCache().OnDestroyDevice();

	DestroyScene();

	SAFE_DELETE(g_TextHelper);
	SAFE_DELETE(g_SSAO);
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
	// Pass messages to dialog resource manager calls so GUI state is updated correctly
	*pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
	if( *pbNoFurtherProcessing )
		return 0;

	// Pass messages to settings dialog if its active
	if( g_D3DSettingsDlg.IsActive() )
	{
		g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
		return 0;
	}

	// Give the dialogs a chance to handle the message first
	*pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
	if( *pbNoFurtherProcessing )
		return 0;

	// Pass all remaining windows messages to camera so it can respond to user input
	g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);

	g_LightAnimation->RecordLight(g_Camera, uMsg, wParam, lParam);

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
                       bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta,
                       int xPos, int yPos, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif


    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMouse( OnMouse );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

    // Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // Perform any application-level initialization here
	InitApp();

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"SSAO" );

    // Only require 10-level hardware
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true,  1280, 720);
    DXUTMainLoop(); // Enter into the DXUT ren  der loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}

void InitScene(ID3D11Device* d3dDevice)
{
	DestroyScene();

	g_LightAnimation = new LightAnimation;
	g_Scene = new Scene;

	D3DXVECTOR3 cameraEye(0.0f, 0.0f, 0.0f);
	D3DXVECTOR3 cameraAt(0.0f, 0.0f, 0.0f);
	D3DXVECTOR3 lightDirection(0.0f, 0.0f, 0.0f);
	float sceneScaling = 1.0f;

	D3DXMATRIX world;

	HRESULT hr;

	SceneSelection scene = static_cast<SceneSelection>(PtrToUlong(g_SceneSelectCombo->GetSelectedData()));
	switch (scene)
	{
	case Scene_Power_Plant: 
		{
			g_LightAnimation->LoadLights(".\\Media\\Animation.txt");
			//g_LightAnimation->RandonPointLight(10);

			sceneScaling = 1.0f;
			D3DXMatrixScaling(&world, sceneScaling, sceneScaling, sceneScaling);
			
			g_Scene->LoadOpaqueMesh(d3dDevice, L"..\\media\\powerplant\\powerplant.sdkmesh", world);

			cameraEye = sceneScaling * D3DXVECTOR3(100.0f, 5.0f, 5.0f);
			cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);		
		} 
		break;

	case Scene_Sponza: 
		{
			g_LightAnimation->LoadLights(".\\Media\\Animation.txt");
			//g_LightAnimation->RandonPointLight(100);

			sceneScaling = 0.05f;
			D3DXMatrixScaling(&world, sceneScaling, sceneScaling, sceneScaling);

			g_Scene->LoadOpaqueMesh(d3dDevice, L".\\Media\\Sponza\\sponza_dds.sdkmesh", world);

			cameraEye = sceneScaling * D3DXVECTOR3(1200.0f, 200.0f, 100.0f);
			cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		} 
		break;
	};

	g_Camera.SetViewParams(&cameraEye, &cameraAt);
	g_Camera.SetScalers(0.01f, 10.0f);
	g_Camera.FrameMove(0.0f);
}

void DestroyScene()
{
	if (g_Scene)
	{
		delete g_Scene;
		g_Scene = NULL;
	}

	if (g_LightAnimation)
	{
		g_LightAnimation->SaveLights();
		delete g_LightAnimation;
		g_LightAnimation = NULL;
	}	
}

void InitUI()
{
	// Initialize dialogs
	g_D3DSettingsDlg.Init( &g_DialogResourceManager );
	g_HUD.Init( &g_DialogResourceManager );
	g_HUD.SetCallback(OnGUIEvent);

	int iY = 10, width = 170;
	g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, width, 23 );
	g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 36, width, 23, VK_F3 );
	g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 36, width, 23, VK_F2 );
	g_HUD.AddComboBox(IDC_SCENE_SELECTION, 0, iY +=36, width, 23, 0, false, &g_SceneSelectCombo);
	g_SceneSelectCombo->AddItem(L"Power Plant", ULongToPtr(Scene_Power_Plant));
	g_SceneSelectCombo->AddItem(L"Sponza", ULongToPtr(Scene_Sponza));
	g_HUD.AddComboBox(IDC_SHADING_SELECTION, 0, iY +=36, width, 23, 0, false, &g_ShadingSelectCombo);
	g_ShadingSelectCombo->AddItem(L"Forward", ULongToPtr(Shading_Forward));
	g_ShadingSelectCombo->AddItem(L"Deferred Shading", ULongToPtr(Shading_DeferredShading));

	g_HUD.SetSize(width, iY);
}
