// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "SimulPrecipitationRenderer.h"

#ifdef XBOX
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	#include <tchar.h>
	#include <d3d9.h>
	#include <d3dx9.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif

#include "Simul/Base/SmartPtr.h"
#include "Macros.h"
#include "CreateDX9Effect.h"
#include "Resources.h"

typedef std::basic_string<TCHAR> tstring;

SimulPrecipitationRenderer::SimulPrecipitationRenderer() :
	m_pVtxDecl(NULL),
	m_pRainEffect(NULL),
	rain_texture(NULL),
	radius(10.f),
	height(100.f),
	rain_intensity(0.f),
	external_rain_texture(false)
{
}

struct Vertex_t
{
	float x,y,z;
	float tex_x,tex_y,fade;
};

#define CONE_SIDES 36
#define NUM_VERT ((CONE_SIDES+1)*4)
static Vertex_t vertices[NUM_VERT];

HRESULT SimulPrecipitationRenderer::RestoreDeviceObjects( LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);

	int index=0;
	for(int j=-1;j<=1;j+=2)
	for(int i=0;i<CONE_SIDES+1;i++)
	{
		float angle1=2.f*3.14159f*(float)i/(float)CONE_SIDES;
		vertices[index].x=radius*cos(angle1);
		vertices[index].z=radius*sin(angle1);
		vertices[index].y=0;
		vertices[index].tex_x=i*14.f/(float)CONE_SIDES;
		vertices[index].tex_y=0;
		vertices[index].fade=1.f;
		index++;
		vertices[index].x=0;
		vertices[index].z=0;
		vertices[index].y=height*j;
		vertices[index].tex_x=i*14.f/(float)CONE_SIDES;
		vertices[index].tex_y=(float)j;
		vertices[index].fade=0.f;
		index++;
	}
	D3DVERTEXELEMENT9 decl[]=
	{
		{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);
	V_RETURN(CreateDX9Effect(m_pd3dDevice,m_pRainEffect,"simul_rain.fx"));

	m_hTechniqueRain	=m_pRainEffect->GetTechniqueByName("simul_rain");
	worldViewProj		=m_pRainEffect->GetParameterByName(NULL,"worldViewProj");
	offset				=m_pRainEffect->GetParameterByName(NULL,"offset");
	intensity			=m_pRainEffect->GetParameterByName(NULL,"intensity");
	lightColour			=m_pRainEffect->GetParameterByName(NULL,"lightColour");

	if(!external_rain_texture)
	{
		SAFE_RELEASE(rain_texture);
		V_RETURN(hr=CreateDX9Texture(m_pd3dDevice,rain_texture,"Rain.jpg"));
	}

	return hr;
}

HRESULT SimulPrecipitationRenderer::SetExternalRainTexture(LPDIRECT3DTEXTURE9 tex)
{
	if(!external_rain_texture)
	{
		SAFE_RELEASE(rain_texture);
	}
	external_rain_texture=true;
	rain_texture=tex;
	return S_OK;
}

HRESULT SimulPrecipitationRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pRainEffect)
        hr=m_pRainEffect->OnLostDevice();
	SAFE_RELEASE(m_pRainEffect);
	SAFE_RELEASE(m_pVtxDecl);
	if(!external_rain_texture)
		SAFE_RELEASE(rain_texture);
	return hr;
}

HRESULT SimulPrecipitationRenderer::Destroy()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pRainEffect);
	if(!external_rain_texture)
		SAFE_RELEASE(rain_texture);
	return hr;
}

SimulPrecipitationRenderer::~SimulPrecipitationRenderer()
{
	Destroy();
}
	static const float radius=50.f;
	static const float height=150.f;

HRESULT SimulPrecipitationRenderer::Render()
{
	if(rain_intensity<=0)
		return S_OK;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif

	PIXBeginNamedEvent(0,"Render Precipitation");
	m_pd3dDevice->SetTexture(0,rain_texture);
	/*m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);*/

	HRESULT hr=m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl);

	/*m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    hr=m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);*/

	m_pRainEffect->SetTechnique( m_hTechniqueRain );

	D3DXMatrixIdentity(&world);
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	//set up matrices
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pRainEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));
	m_pRainEffect->SetFloat(offset,offs);
	m_pRainEffect->SetFloat(intensity,rain_intensity);

	m_pRainEffect->SetVector(lightColour,(D3DXVECTOR4*)(light_colour));

	UINT passes=1;
	hr=m_pRainEffect->Begin( &passes, 0 );
	for(unsigned i = 0 ; i < passes ; ++i )
	{
		hr=m_pRainEffect->BeginPass(i);
		hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,NUM_VERT-2,vertices,sizeof(Vertex_t));
		hr=m_pRainEffect->EndPass();
	}
	hr=m_pRainEffect->End();
	D3DXMatrixIdentity(&world);
	PIXEndNamedEvent();
	return hr;
}

void SimulPrecipitationRenderer::SetLightColour(const float c[4])
{
	static float cc=0.02f;
	light_colour[0]=cc*c[0];
	light_colour[1]=cc*c[1];
	light_colour[2]=cc*c[2];
	light_colour[3]=1.f;
}

#ifdef XBOX
void SimulPrecipitationRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}
#endif

void SimulPrecipitationRenderer::Update(float dt)
{
	static bool pause=false;
		static float cc=1.6f;
    if(!pause)
	{
		offs+=cc*dt;
	}
}