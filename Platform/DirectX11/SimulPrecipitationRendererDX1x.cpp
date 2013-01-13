// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "SimulPrecipitationRendererDX1x.h"
#include "Simul/Base/SmartPtr.h"
#include "CreateEffectDX1x.h"
#include "FramebufferDX1x.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = x; if( FAILED(hr) ) { return hr; } }
#endif

typedef std::basic_string<TCHAR> tstring;

SimulPrecipitationRendererDX1x::SimulPrecipitationRendererDX1x() :
	m_pd3dDevice(NULL)
	,m_pImmediateContext(NULL)
	,m_pVtxDecl(NULL)
	,m_pVertexBuffer(NULL)
	,m_pRainEffect(NULL)
	,rain_texture(NULL)
	,pShadingCB(NULL)
{
}

struct Vertex_t
{
	float x,y,z;
	float tex_x,tex_y,fade;
};

void SimulPrecipitationRendererDX1x::RecompileShaders()
{
	CreateEffect(m_pd3dDevice,&m_pRainEffect,L"simul_rain.fx");

	SAFE_RELEASE(rain_texture);
	//rain_texture		=simul::dx11::LoadTexture("Rain.png");
	m_hTechniqueRain	=m_pRainEffect->GetTechniqueByName("simul_rain");
	worldViewProj		=m_pRainEffect->GetVariableByName("worldViewProj")->AsMatrix();
	offset				=m_pRainEffect->GetVariableByName("offset")->AsScalar();
	intensity			=m_pRainEffect->GetVariableByName("intensity")->AsScalar();
	lightColour			=m_pRainEffect->GetVariableByName("lightColour")->AsVector();
	lightDir			=m_pRainEffect->GetVariableByName("lightDir")->AsVector();
	rainTexture			=m_pRainEffect->GetVariableByName("rainTexture")->AsShaderResource();


	SAFE_RELEASE(pShadingCB);
/*	D3D11_SUBRESOURCE_DATA cb_init_data;
	cb_init_data.pSysMem = &rainConstantBuffer;
	cb_init_data.SysMemPitch = 0;
	cb_init_data.SysMemSlicePitch = 0;

	// Constants
	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage				= D3D11_USAGE_DYNAMIC;
	cb_desc.BindFlags			= D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags		= D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags			= 0;    
	cb_desc.ByteWidth			= PAD16(sizeof(RainConstantBuffer));
	cb_desc.StructureByteStride = 0;
	m_pd3dDevice->CreateBuffer(&cb_desc, &cb_init_data, &pShadingCB);*/
	MAKE_CONSTANT_BUFFER(pShadingCB,RainConstantBuffer);

	ID3D1xEffectTechnique*			tech	=m_pRainEffect->GetTechniqueByName("create_rain_texture");
	ApplyPass(tech->GetPassByIndex(0));
	FramebufferDX1x make_rain_fb(512,512);
	make_rain_fb.RestoreDeviceObjects(m_pd3dDevice);
	make_rain_fb.SetTargetWidthAndHeight(512,512);
	make_rain_fb.Activate();
	make_rain_fb.DrawQuad();
	make_rain_fb.Deactivate();
	rain_texture=make_rain_fb.buffer_texture_SRV;
	// Make sure it isn't destroyed when the fb goes out of scope:
	rain_texture->AddRef();
}

void SimulPrecipitationRendererDX1x::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	MakeMesh();
    RecompileShaders();
	D3D1x_INPUT_ELEMENT_DESC decl[] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT	,0,0	,D3D1x_INPUT_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12	,D3D1x_INPUT_PER_VERTEX_DATA,0},
	};
	SAFE_RELEASE(m_pVtxDecl);
    D3D1x_PASS_DESC PassDesc;
	m_hTechniqueRain->GetPassByIndex(0)->GetDesc(&PassDesc);
	hr=m_pd3dDevice->CreateInputLayout(decl,2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pVtxDecl);
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(Vertex_t);

	D3D1x_BUFFER_DESC desc=
	{
        NUM_VERT*sizeof(Vertex_t),
        D3D1x_USAGE_DEFAULT,
        D3D1x_BIND_VERTEX_BUFFER,
        0,
        0
	};
	m_pd3dDevice->CreateBuffer(&desc,&InitData,&m_pVertexBuffer);
}


void SimulPrecipitationRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(m_pRainEffect);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(rain_texture);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(pShadingCB);
}


SimulPrecipitationRendererDX1x::~SimulPrecipitationRendererDX1x()
{
	InvalidateDeviceObjects();
}

/*
Here, we only consider the effect of exposure time on the transparency of the rain streak. It has been
shown [Garg and Nayar 2005] that the intensity Ir at a rain-affected
pixel is given by Ir =(1-a) Ib+a Istreak, where Ib and Istreak are the
intensities of the background and the streak at a rain-effected pixel.
The transparency factor a depends on drop size r0 and velocity v
and is given by a = 2 r0/v Texp. Also, the rain streaks become more
opaque with shorter exposure time. In the last step, we use the userspecified
depth map of the scene to find the pixels for which the rain
streak is not occluded by the scene. The streak is rendered only over
those pixels.
*/
void SimulPrecipitationRendererDX1x::Render()
{
return;
	if(rain_intensity<=0)
		return;
	PIXBeginNamedEvent(0,"Render Precipitation");
	rainTexture->SetResource(rain_texture);
/*	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
*/
	m_pImmediateContext->IASetInputLayout( m_pVtxDecl );

/*	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    hr=m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
*/
	//set up matrices
	D3DXMATRIX tmp1,tmp2,world,wvp;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	D3DXMatrixIdentity(&world);
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
	worldViewProj->SetMatrix(&wvp._11);
	offset->SetFloat(offs);
	intensity->SetFloat(rain_intensity);

	lightColour->SetFloatVector((const float*)(light_colour));
	lightDir->SetFloatVector((const float*)(light_dir));
	
	rainConstantBuffer.intensity		=rain_intensity;
	rainConstantBuffer.lightColour		=(const float*)light_colour;
	rainConstantBuffer.lightDir			=(const float*)light_dir;
	rainConstantBuffer.offset			=offs;
	rainConstantBuffer.worldViewProj	=wvp;
	// Update constant buffer
	D3D11_MAPPED_SUBRESOURCE mapped_res;            
	m_pImmediateContext->Map(pShadingCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
	*(RainConstantBuffer*)mapped_res.pData = rainConstantBuffer;
	m_pImmediateContext->Unmap(pShadingCB, 0);

	m_pImmediateContext->PSSetConstantBuffers(0, 1, &pShadingCB);
	//m_pImmediateContext->PSSetConstantBuffers( 3, 1, bufferPS);

	UINT passes=1;
	for(unsigned i = 0 ; i < passes ; ++i )
	{
		ApplyPass(m_hTechniqueRain->GetPassByIndex(i));

		m_pImmediateContext->IASetInputLayout( m_pVtxDecl );
		UINT stride = sizeof(Vertex_t);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = 0;
		Offsets[0] = 0;
		m_pImmediateContext->IASetVertexBuffers(	0,					// the first input slot for binding
													1,					// the number of buffers in the array
													&m_pVertexBuffer,	// the array of vertex buffers
													&stride,			// array of stride values, one for each buffer
													&offset );

		m_pImmediateContext->IASetInputLayout(m_pVtxDecl);

		m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		m_pImmediateContext->Draw(NUM_VERT-2,0);
	}
	//hr=m_pRainEffect->End();
	//m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  //  hr=m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
	D3DXMatrixIdentity(&world);
	PIXEndNamedEvent();
}

void SimulPrecipitationRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}