// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulAtmosphericsRendererDX1x.cpp A renderer for skies, clouds and weather effects.

#include "SimulAtmosphericsRendererDX1x.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static DWORD default_effect_flags=0;
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#endif
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRendererDX1x.h"
#include "SimulPrecipitationRendererDX1x.h"
#include "Simul2DCloudRendererDX1x.h"
#include "SimulSkyRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "CreateEffectDX1x.h"


#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)		{ if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)			{ hr = x; if( FAILED(hr) ) { return hr; } }
#endif
#ifndef SAFE_DELETE
#define SAFE_DELETE(p)		{ if(p) { delete (p);     (p)=NULL; } }
#endif    
#define BLUR_SIZE 9
#define MONTE_CARLO_BLUR

#ifdef  MONTE_CARLO_BLUR
#include "Simul/Math/Pi.h"
#endif

SimulAtmosphericsRendererDX1x::SimulAtmosphericsRendererDX1x() :
	m_pd3dDevice(NULL),
	vertexDecl(NULL),
	framebuffer(NULL),
	effect(NULL),
	lightDir(NULL),
	m_pImmediateContext(NULL),
	MieRayleighRatio(NULL),
	HazeEccentricity(NULL),
	fadeInterp(NULL),
	imageTexture(NULL),
	depthTexture(NULL),
	lossTexture1(NULL),
	inscatterTexture1(NULL),
	loss_texture(NULL),
	inscatter_texture(NULL),
	skylight_texture(NULL),
	clouds_texture(NULL),
	fade_interp(0.f)
{
	framebuffer=new FramebufferDX1x(256,256);
}

SimulAtmosphericsRendererDX1x::~SimulAtmosphericsRendererDX1x()
{
	Destroy();
	delete framebuffer;
}

void SimulAtmosphericsRendererDX1x::SetBufferSize(int w,int h)
{
	if(framebuffer)
		framebuffer->SetWidthAndHeight(w,h);
	InvalidateDeviceObjects();
}

void SimulAtmosphericsRendererDX1x::SetYVertical(bool y)
{
}

void SimulAtmosphericsRendererDX1x::SetDistanceTexture(void* t)
{
	depth_texture=(ID3D1xTexture2D*)t;
}

void SimulAtmosphericsRendererDX1x::SetLossTexture(void* t)
{
	loss_texture=(ID3D1xTexture2D*)t;
}

void SimulAtmosphericsRendererDX1x::SetInscatterTextures(void* t,void *s)
{
	inscatter_texture=(ID3D1xTexture2D*)t;
	skylight_texture=(ID3D1xTexture2D*)s;
}


void SimulAtmosphericsRendererDX1x::SetCloudsTexture(void* t)
{
	clouds_texture=(ID3D1xTexture2D*)t;
}

void SimulAtmosphericsRendererDX1x::RecompileShaders()
{
	SAFE_RELEASE(effect);
	HRESULT hr=S_OK;
	V_CHECK(CreateEffect(m_pd3dDevice,&effect,L"atmospherics.fx"));

	technique			=effect->GetTechniqueByName("simul_atmospherics");
	
	invViewProj			=effect->GetVariableByName("invViewProj")->AsMatrix();
	lightDir			=effect->GetVariableByName("lightDir")->AsVector();
	MieRayleighRatio	=effect->GetVariableByName("MieRayleighRatio")->AsVector();
	HazeEccentricity	=effect->GetVariableByName("HazeEccentricity")->AsScalar();
	fadeInterp			=effect->GetVariableByName("fadeInterp")->AsScalar();
	imageTexture		=effect->GetVariableByName("imageTexture")->AsShaderResource();
	depthTexture		=effect->GetVariableByName("depthTexture")->AsShaderResource();
	lossTexture1		=effect->GetVariableByName("lossTexture1")->AsShaderResource();
	inscatterTexture1	=effect->GetVariableByName("inscatterTexture1")->AsShaderResource();
}

HRESULT SimulAtmosphericsRendererDX1x::RestoreDeviceObjects(ID3D1xDevice* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=dev;
#ifdef DX10
	m_pImmediateContext=dev;
#else
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	RecompileShaders();
	if(framebuffer)
		framebuffer->RestoreDeviceObjects(dev);
/*	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0,  8, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#endif
		D3DDECL_END()
	};
	SAFE_RELEASE(vertexDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&vertexDecl);*/
	return hr;
}

HRESULT SimulAtmosphericsRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(framebuffer)
		framebuffer->InvalidateDeviceObjects();
#ifndef DX10
	SAFE_RELEASE(m_pImmediateContext);
#endif
	SAFE_RELEASE(vertexDecl);
	SAFE_RELEASE(effect);
	return hr;
}

HRESULT SimulAtmosphericsRendererDX1x::Destroy()
{
	return InvalidateDeviceObjects();
}

void SimulAtmosphericsRendererDX1x::SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	world=w;
	view=v;
	proj=p;
}

void SimulAtmosphericsRendererDX1x::StartRender()
{
	if(!framebuffer)
		return;
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::StartRender");
	framebuffer->Activate();
	// Clear the screen to black:
    float clearColor[4]={0.0,0.0,0.0,0.0};
	m_pImmediateContext->ClearRenderTargetView(framebuffer->m_pHDRRenderTarget,clearColor);
	if(framebuffer->m_pBufferDepthSurface)
		m_pImmediateContext->ClearDepthStencilView(framebuffer->m_pBufferDepthSurface,D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL, 1.f, 0);
	PIXEndNamedEvent();
}

void SimulAtmosphericsRendererDX1x::FinishRender()
{
	if(!framebuffer)
		return;
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::FinishRender");
	framebuffer->DeactivateAndRender(false);
	PIXEndNamedEvent();
}
