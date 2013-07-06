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

#include "Renderer.h"
#include "Scene.h"
#include "LightAnimation.h"

#include <sstream>

CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CDXUTDialog                 g_HUD;                  // manages the 3D   
CDXUTDialog                 g_HbaoHUD;                  // manages the 3D   


CD3DSettingsDlg             g_D3DSettingsDlg;       // Device settings dialog
CDXUTTextHelper*            g_TextHelper;
CFirstPersonCamera          g_Camera;               // A FPS camera

Renderer*                   g_Renderer;
Scene*                      g_Scene;
LightAnimation*             g_LightAnimation;


enum SceneSelection
{
	Scene_Power_Plant,
	Scene_Sponza,
};

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN			1
#define IDC_TOGGLEREF					3
#define IDC_CHANGEDEVICE				4
#define IDC_SCENE_SELECTION				5
#define IDC_SHADING_SELECTION			6
#define IDC_DEFERRED_LIGHTING			7
#define IDC_LIGHT_CULL					8
#define IDC_LIGHT_ANIMATION				9
#define IDC_STATIC_AO_RADIUS			10
#define IDC_SLIDER_AO_RADIUS            11
#define IDC_STATIC_AO_BAISANGLE			12
#define IDC_SLIDER_AO_BAISANGLE         13
#define IDC_STATIC_BLUR_WIDTH			14
#define IDC_SLIDER_BLUR_WIDTH	        15
#define IDC_STATIC_BLUR_SHARPNESS		16
#define IDC_SLIDER_BLUR_SHARPNESS       17
#define IDC_COMBOBOX_AO					18 
#define IDC_USE_AO                      19
#define IDC_SHOW_AO                     20

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
#define Lerp(v, min, max) ((min)*(1 - (v)) + (max)*(v))

	WCHAR szTemp[256];

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
	case IDC_DEFERRED_LIGHTING:
		{
			if(g_Renderer) 
				g_Renderer->mLightPrePass = g_HUD.GetCheckBox(IDC_DEFERRED_LIGHTING)->GetChecked();
		}
		break;
	case IDC_LIGHT_CULL:
		{
			if(g_Renderer) 
				g_Renderer->mCullTechnique = static_cast<LightCullTechnique>(
				PtrToUlong(g_HUD.GetComboBox(IDC_LIGHT_CULL)->GetSelectedData()));
		}
		break;
	case IDC_SHADING_SELECTION:
		{
			if(g_Renderer) 
				g_Renderer->mLightingMethod = static_cast<LightingMethod>(
				PtrToUlong(g_HUD.GetComboBox(IDC_SHADING_SELECTION)->GetSelectedData()));
		}
		break;
	case IDC_SLIDER_AO_RADIUS:
		{
			float value = (float)g_HbaoHUD.GetSlider( IDC_SLIDER_AO_RADIUS )->GetValue() / 100.0f;

			// Map to [0, 5]
		    float min = 1;
			float max = 5;
			
			value = Lerp(value, min, max);
			swprintf_s( szTemp, L"AO Radius: %.1f", value );
			g_HbaoHUD.GetStatic( IDC_STATIC_AO_RADIUS )->SetText( szTemp );

			if (g_Renderer)
				g_Renderer->mHBAOParams.Radius = value;
		}
		break;
	case IDC_SLIDER_AO_BAISANGLE:
		{
			float value = (float)g_HbaoHUD.GetSlider( IDC_SLIDER_AO_BAISANGLE )->GetValue() / 100.0f;

			// Map to [0, 60]
			float min = 0;
			float max = 60;
			value = Lerp(value, min, max);

			swprintf_s( szTemp, L"AO Bais Angle: %.1f", value );
			g_HbaoHUD.GetStatic( IDC_STATIC_AO_BAISANGLE)->SetText( szTemp );

			if (g_Renderer)
				g_Renderer->mHBAOParams.TanAngleBias = tanf(D3DXToRadian(value));;
		}
		break;
	case IDC_SLIDER_BLUR_WIDTH:
		{
			int value = g_HbaoHUD.GetSlider( IDC_SLIDER_BLUR_WIDTH )->GetValue();

			swprintf_s( szTemp, L"Blur Size: %d", 2*value+1 );
			g_HbaoHUD.GetStatic( IDC_STATIC_BLUR_WIDTH)->SetText( szTemp );

			if (g_Renderer)
				g_Renderer->mBlurParams.BlurRadius = (float)value;
		}
		break;
	case IDC_SLIDER_BLUR_SHARPNESS:
		{
			int sharpness = g_HbaoHUD.GetSlider( IDC_SLIDER_BLUR_SHARPNESS )->GetValue();

			swprintf_s( szTemp, L"Blur Sharpness: %d",sharpness);
			g_HbaoHUD.GetStatic( IDC_STATIC_BLUR_SHARPNESS)->SetText( szTemp );

			if (g_Renderer)
				g_Renderer->mBlurParams.BlurSharpness = sharpness * sharpness ;
		}
		break;
	case IDC_COMBOBOX_AO:
		{
			if (g_Renderer)
			{
				g_Renderer->mAOTechnique = static_cast<AmbientOcclusionTechnique>(PtrToUlong(g_HUD.GetComboBox(IDC_COMBOBOX_AO)->GetSelectedData()));	
				
				//g_HbaoHUD.SetVisible(g_Renderer->mAOTechnique == AO_HBAO);
			}
		}
		break;
	case IDC_USE_AO:
		{
			if(g_Renderer) 
				g_Renderer->mUseSSAO = g_HUD.GetCheckBox(IDC_USE_AO)->GetChecked();
		}
		break;
	case IDC_SHOW_AO:
		{
			if(g_Renderer) 
				g_Renderer->mShowAO = g_HUD.GetCheckBox(IDC_SHOW_AO)->GetChecked();
		}
		break;
	}

#undef Lerp
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
	if (!g_Renderer)
		g_Renderer = new Renderer(pd3dDevice);

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
	g_HbaoHUD.SetLocation( pBackBufferSurfaceDesc->Width - g_HbaoHUD.GetWidth() - border, g_HUD.GetHeight() );

	if (g_Renderer)
		g_Renderer->OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc);
	
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	g_Camera.FrameMove(fElapsedTime);

	bool lightAnimated = g_HUD.GetCheckBox(IDC_LIGHT_ANIMATION)->GetChecked();
	if (lightAnimated && g_LightAnimation)
		g_LightAnimation->Move(fElapsedTime);
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

	g_Renderer->Render(pd3dImmediateContext, pRTV, pDSV, *g_Scene, *g_LightAnimation, g_Camera, &viewport);

	// reset render target
	pd3dImmediateContext->RSSetViewports(1, &viewport);
	pd3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDSV);

	g_HUD.OnRender( fElapsedTime );
	g_HbaoHUD.OnRender(fElapsedTime);
	//AmbientOcclusionTechnique aoTech = static_cast<AmbientOcclusionTechnique>((PtrToUlong(g_HUD.GetComboBox(IDC_COMBOBOX_AO)->GetSelectedData())));


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
	SAFE_DELETE(g_Renderer);
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

	*pbNoFurtherProcessing = g_HbaoHUD.MsgProc( hWnd, uMsg, wParam, lParam );
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
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true,  1280, 720);
    DXUTMainLoop(); // Enter into the DXUT render loop

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

	SceneSelection scene = static_cast<SceneSelection>(PtrToUlong(g_HUD.GetComboBox(IDC_SCENE_SELECTION)->GetSelectedData()));
	switch (scene)
	{
	case Scene_Power_Plant: 
		{
			g_LightAnimation->LoadLights(".\\Media\\Animation.txt");
			g_LightAnimation->RandonPointLight(100);

			sceneScaling = 1.0f;
			D3DXMatrixScaling(&world, sceneScaling, sceneScaling, sceneScaling);
			
			g_Scene->LoadOpaqueMesh(d3dDevice, L"..\\media\\powerplant\\powerplant.sdkmesh", world);

			D3DXVECTOR3 extent = g_Scene->mWorldBound.Extent();
			D3DXVECTOR3 center = g_Scene->mWorldBound.Center();

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

			D3DXVECTOR3 extent = g_Scene->mWorldBound.Extent();
			D3DXVECTOR3 center = g_Scene->mWorldBound.Center();

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
#define Default(v, min, max) int((v - min) / (max - min ) * 100.0f )

	WCHAR szTemp[256];

	// Initialize dialogs
	g_D3DSettingsDlg.Init( &g_DialogResourceManager );
	g_HUD.Init( &g_DialogResourceManager );
	g_HbaoHUD.Init( &g_DialogResourceManager );

	g_HUD.SetCallback(OnGUIEvent);
	g_HbaoHUD.SetCallback(OnGUIEvent);

	//D3DCOLOR DlgColor = 0x88888888;
	//g_HUD.SetBackgroundColors(DlgColor);

	CDXUTComboBox *pCombo;
	CDXUTCheckBox *pCheck;

	int iY = 20, width = 200;
	g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, width, 23 );
	g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 36, width, 23, VK_F3 );
	g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 36, width, 23, VK_F2 );
	
	g_HUD.AddComboBox(IDC_SCENE_SELECTION, 0, iY +=36, width, 23, 0, false, &pCombo);
	pCombo->SetDropHeight( 15 );
	pCombo->AddItem(L"Power Plant", ULongToPtr(Scene_Power_Plant));
	pCombo->AddItem(L"Sponza", ULongToPtr(Scene_Sponza));
	pCombo->SetSelectedByIndex(Scene_Sponza);

	iY +=36;
	g_HUD.AddComboBox(IDC_SHADING_SELECTION, 0, iY +=36, width, 23, 0, false, &pCombo);
	pCombo->SetDropHeight( 15 );
	pCombo->AddItem(L"Forward", ULongToPtr(Lighting_Forward));
	pCombo->AddItem(L"Deferred", ULongToPtr(Lighting_Deferred));
	pCombo->SetSelectedByIndex(Lighting_Deferred);

	g_HUD.AddComboBox(IDC_LIGHT_CULL, 0, iY +=36, width, 23, 0, false, &pCombo);
	pCombo->SetDropHeight( 50 );
	pCombo->AddItem(L"Cull_Forward_None", ULongToPtr(Cull_Forward_None));
	pCombo->AddItem(L"Cull_Forward_PreZ_None", ULongToPtr(Cull_Forward_PreZ_None));
	pCombo->AddItem(L"Cull_Deferred_Volume", ULongToPtr(Cull_Deferred_Volume));
	pCombo->AddItem(L"Cull_Deferred_Quad", ULongToPtr(Cull_Deferred_Quad));
	pCombo->AddItem(L"Cull_Deferred_Tile", ULongToPtr(Cull_Deferred_Tile));
	pCombo->SetSelectedByIndex(Cull_Deferred_Volume);

	g_HUD.AddCheckBox(IDC_DEFERRED_LIGHTING, L"Light Pre-Pass", 0, iY += 36, width, 23, 0, false, false, &pCheck);
	pCheck->SetChecked(false);

	g_HUD.AddCheckBox(IDC_LIGHT_ANIMATION, L"Light Animation", 0, iY += 36, width, 23, 0, false, false, &pCheck);
	pCheck->SetChecked(true);

	g_HUD.AddCheckBox(IDC_USE_AO, L"Use SSAO", 0, iY += 36, width, 23, 0, false, false, &pCheck);
	pCheck->SetChecked(true);

	g_HUD.AddCheckBox(IDC_SHOW_AO, L"Show SSAO", 0, iY += 36, width, 23, 0, false, false, &pCheck);
	pCheck->SetChecked(false);

	iY +=16;
	g_HUD.AddComboBox(IDC_COMBOBOX_AO, 0, iY +=36, width, 23, 0, false, &pCombo);
	pCombo->AddItem(L"Cryteck", ULongToPtr(AO_Cryteck));
	pCombo->AddItem(L"HBAO", ULongToPtr(AO_HBAO));
	pCombo->AddItem(L"Unreal4", ULongToPtr(AO_Unreal4));
	pCombo->AddItem(L"Alchemy", ULongToPtr(AO_Alchemy));
	

	g_HUD.SetSize(width, iY);
	
	// HBAO
	{
		D3DCOLOR DlgColor = D3DCOLOR_RGBA(50, 50, 50, 125);
		g_HbaoHUD.SetBackgroundColors(DlgColor);

		int iYY = 0;
		swprintf_s( szTemp, L"AO Radius: %.1f", 1.0f);
		g_HbaoHUD.AddStatic( IDC_STATIC_AO_RADIUS, szTemp, 0, iYY += 36, width, 23);
		g_HbaoHUD.AddSlider(IDC_SLIDER_AO_RADIUS, 0,  iYY += 23, width, 23, 0, 100, Default(1.0f, 0.0f, 5.0f), false);

		swprintf_s( szTemp, L"Bais Angle: %.1f", 10.0f);
		g_HbaoHUD.AddStatic( IDC_STATIC_AO_BAISANGLE, szTemp, 0, iYY += 36, width, 23);
		g_HbaoHUD.AddSlider(IDC_SLIDER_AO_BAISANGLE, 0,  iYY += 23, width, 23, 0, 100, Default(10.0f, 0.0f, 60.0f), false);

		swprintf_s( szTemp, L"Blur Size: %d", 2*7+1);
		g_HbaoHUD.AddStatic( IDC_STATIC_BLUR_WIDTH, szTemp, 0, iYY += 36, width, 23);
		g_HbaoHUD.AddSlider(IDC_SLIDER_BLUR_WIDTH, 0,  iYY += 23, width, 23, 0, 16, 7, false);

		swprintf_s( szTemp, L"Blur Sharpness: %d", 18);
		g_HbaoHUD.AddStatic( IDC_STATIC_BLUR_SHARPNESS, szTemp, 0, iYY += 36, width, 23);
		g_HbaoHUD.AddSlider(IDC_SLIDER_BLUR_SHARPNESS, 0,  iYY += 23, width, 23, 0, 32, 18, false);

		g_HbaoHUD.SetSize(width, iYY);

		//g_HbaoHUD.SetVisible(false);
	}

	
	
#undef Default
}
