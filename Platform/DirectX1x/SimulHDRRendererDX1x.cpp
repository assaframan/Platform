// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulHDRRendererDX1x.cpp A renderer for skies, clouds and weather effects.

#include "SimulHDRRendererDX1x.h"


#include <tchar.h>
#include <dxerr.h>
#include <string>
#include <assert.h>
typedef std::basic_string<TCHAR> tstring;

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRendererDX1x.h"
//#include "SimulPrecipitationRendererdx10.h"
//#include "Simul2DCloudRendererdx10.h"
#include "SimulSkyRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "CreateEffectDX1x.h"
#include "SimulAtmosphericsRendererDX1x.h"
#include "MacrosDX1x.h"


#define BLUR_SIZE 9

#include "Simul/Math/Pi.h"

SimulHDRRendererDX1x::SimulHDRRendererDX1x(int w,int h) :
	m_pd3dDevice(NULL),
	m_pImmediateContext(NULL),
	m_pVertexBuffer(NULL),
	m_pTonemapEffect(NULL),
	Exposure(1.f),
	gamma(1.f),			// no need for shader-based gamma-correction with DX10/11
	Width(w),
	Height(h),
	timing(0.f),
	screen_width(0),
	screen_height(0)
{
	framebuffer=new FramebufferDX1x(w,h);
}

void SimulHDRRendererDX1x::SetBufferSize(int w,int h)
{
	Width=w;
	Height=h;
	framebuffer->SetWidthAndHeight(w,h);
	InvalidateDeviceObjects();
}

bool SimulHDRRendererDX1x::RestoreDeviceObjects(ID3D1xDevice* dev,IDXGISwapChain* pSwapChain)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=dev;
	framebuffer->RestoreDeviceObjects(dev);

	ID3D1xTexture2D *pBackBuffer=NULL;
	pSwapChain->GetBuffer(0,__uuidof(ID3D1xTexture2D),(void**)&pBackBuffer);
	D3D1x_TEXTURE2D_DESC desc;
	pBackBuffer->GetDesc(&desc);
	SAFE_RELEASE(pBackBuffer);

	framebuffer->SetTargetWidthAndHeight(desc.Width,desc.Height);
#ifdef DX10
	m_pImmediateContext=dev;
#else
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	SAFE_RELEASE(m_pTonemapEffect);
	if(!m_pTonemapEffect)
	{
		V_CHECK(CreateEffect(m_pd3dDevice,&m_pTonemapEffect,_T("gamma.fx")));
	}
	TonemapTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_tonemap");
	Exposure_param			=m_pTonemapEffect->GetVariableByName("exposure")->AsScalar();
	Gamma					=m_pTonemapEffect->GetVariableByName("gamma")->AsScalar();
	hdrTexture				=m_pTonemapEffect->GetVariableByName("hdrTexture")->AsShaderResource();
	worldViewProj			=m_pTonemapEffect->GetVariableByName("worldViewProj")->AsMatrix();

	return (hr==S_OK);
}

bool SimulHDRRendererDX1x::InvalidateDeviceObjects()
{
	framebuffer->InvalidateDeviceObjects();
	HRESULT hr=S_OK;
#ifndef DX10
	SAFE_RELEASE(m_pImmediateContext);
#endif
	SAFE_RELEASE(m_pTonemapEffect);
	SAFE_RELEASE(m_pVertexBuffer);

	SAFE_RELEASE(m_pTonemapEffect);

	return (hr==S_OK);
}

bool SimulHDRRendererDX1x::Destroy()
{
	return InvalidateDeviceObjects();
}

SimulHDRRendererDX1x::~SimulHDRRendererDX1x()
{
	Destroy();
	delete framebuffer;
}

bool SimulHDRRendererDX1x::StartRender()
{
	framebuffer->SetExposure(Exposure);
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::StartRender");
	framebuffer->Activate();
	// Clear the screen to black:
    float clearColor[4]={0.0,0.0,0.0,0.0};
	m_pImmediateContext->ClearRenderTargetView(framebuffer->m_pHDRRenderTarget,clearColor);
	if(framebuffer->m_pBufferDepthSurface)
		m_pImmediateContext->ClearDepthStencilView(framebuffer->m_pBufferDepthSurface,D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL, 1.f, 0);
	PIXEndNamedEvent();
	return true;
}

bool SimulHDRRendererDX1x::ApplyFade()
{
	HRESULT hr=S_OK;
	return (hr==S_OK);
}

bool SimulHDRRendererDX1x::FinishRender()
{
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::FinishRender");
	framebuffer->DeactivateAndRender(false);
	PIXEndNamedEvent();
	return true;
}

const char *SimulHDRRendererDX1x::GetDebugText() const
{
	static char debug_text[256];
	return debug_text;
}

float SimulHDRRendererDX1x::GetTiming() const
{
	return timing;
}