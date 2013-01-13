// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "SimulTerrainRendererDX1x.h"

#include <dxerr.h>
#include <string>
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
	struct TerrainVertex_t
	{
		float pos[3];
		float n[3];
		float t[2];
		float u;
	};
static const int MAX_VERTICES=1000;

SimulTerrainRendererDX1x::SimulTerrainRendererDX1x()
	:m_pd3dDevice(NULL)
	,m_pImmediateContext(NULL)
	,m_pVertexBuffer(NULL)
	,m_pVtxDecl(NULL)
	,m_pTerrainEffect(NULL)
	,m_pTechnique(NULL)
	,worldViewProj(NULL)
{
}

SimulTerrainRendererDX1x::~SimulTerrainRendererDX1x()
{
}

void SimulTerrainRendererDX1x::RecompileShaders()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pTerrainEffect);
	V_CHECK(CreateEffect(m_pd3dDevice,&m_pTerrainEffect,_T("simul_terrain.fx")));
	
	m_pTechnique			=m_pTerrainEffect->GetTechniqueByName("simul_terrain");
	worldViewProj			=m_pTerrainEffect->GetVariableByName("worldViewProj")->AsMatrix();
}

void SimulTerrainRendererDX1x::RestoreDeviceObjects(void *x)
{
	HRESULT hr=S_OK;
	void **u=(void**)x;
	m_pd3dDevice=(ID3D1xDevice*)u[0];
#ifdef DX10
	m_pImmediateContext=dev;
#else
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	RecompileShaders();

//	ID3D1xBuffer*						m_pVertexBuffer;



	const D3D1x_INPUT_ELEMENT_DESC decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	1, DXGI_FORMAT_R32G32_FLOAT,		0,	24,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	2, DXGI_FORMAT_R32_FLOAT,			0,	32,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
    };
	D3D1x_PASS_DESC PassDesc;
	ID3D1xEffectPass *pass=m_pTechnique->GetPassByIndex(0);
	hr=pass->GetDesc(&PassDesc);
//return true;
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
	// Create the main vertex buffer:
	D3D1x_BUFFER_DESC desc=
	{
        MAX_VERTICES*sizeof(TerrainVertex_t),
        D3D1x_USAGE_DYNAMIC,
        D3D1x_BIND_VERTEX_BUFFER,
        D3D1x_CPU_ACCESS_WRITE,
        0
	};
	TerrainVertex_t *vertices=new TerrainVertex_t[MAX_VERTICES];
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(TerrainVertex_t);
	hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&m_pVertexBuffer);
	delete [] vertices;
}

void SimulTerrainRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
#ifndef DX10
	SAFE_RELEASE(m_pImmediateContext);
#endif
	SAFE_RELEASE(m_pTerrainEffect);
	SAFE_RELEASE(m_pVertexBuffer);
}

void SimulTerrainRendererDX1x::Render()
{
	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	D3DXMATRIX wvp;
	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
	
	TerrainVertex_t *vertices;
	D3D11_MAPPED_SUBRESOURCE mapped_vertices;
	MapBuffer(m_pVertexBuffer,&mapped_vertices);
	vertices=(TerrainVertex_t*)mapped_vertices.pData;
	
	int h=heightMapInterface->GetPageSize();
	
	int v=0;
	for(int i=0;i<h-1;i++)
	{
		float x1=(i-h/2)*1000.f;
		float x2=(i-h/2+1)*1000.f;
		for(int j=0;j<h-1;j++)
		{
			int J=j;
			if(i%2)
				J=(h-2-j);
			float y=(J-h/2)*1000.f;
			float z1=heightMapInterface->GetHeightAt(i,J);
			float z2=heightMapInterface->GetHeightAt(i+1,J);
			simul::math::Vector3 X1(x1,y,z1);
			simul::math::Vector3 X2(x2,y,z2);
			if(v>=1000)
				break;
			TerrainVertex_t &vertex=vertices[v];
			if(i%2==1)
				std::swap(X1,X2);
			vertex.pos[0]=X1.x;
			vertex.pos[1]=X1.y;
			vertex.pos[2]=X1.z;
			vertex.n[0]=0.f;
			vertex.n[1]=0.f;
			vertex.n[2]=1.f;
			v++;
			vertex.pos[0]=X2.x;
			vertex.pos[1]=X2.y;
			vertex.pos[2]=X2.z;
			vertex.n[0]=0.f;
			vertex.n[1]=0.f;
			vertex.n[2]=1.f;
			v++;
		}
	}
	UnmapBuffer(m_pVertexBuffer);
	UINT stride = sizeof(TerrainVertex_t);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	m_pImmediateContext->IASetVertexBuffers(	0,				// the first input slot for binding
												1,				// the number of buffers in the array
												&m_pVertexBuffer,	// the array of vertex buffers
												&stride,		// array of stride values, one for each buffer
												&offset);		// array of offset values, one for each buffer
	ApplyPass(m_pTechnique->GetPassByIndex(0));
	// Set the input layout
	m_pImmediateContext->IASetInputLayout(m_pVtxDecl);
	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	if((v)>2)
		m_pImmediateContext->Draw((v)-2,0);
}

void SimulTerrainRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}