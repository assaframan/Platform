// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRendererDX11.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRendererDX11.h"

#include <dxerr.h>
#include <string>

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Base/StringToWString.h"
#include "SimulSkyRendererDX1x.h"
#include "SimulAtmosphericsRendererDX1x.h"
#include "SimulPrecipitationRendererDX1x.h"

#include "SimulCloudRendererDX1x.h"
#include "Simul2DCloudRendererDX1x.h"
#include "SimulLightningRendererDX11.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"
using namespace simul;
using namespace dx11;

SimulWeatherRendererDX11::SimulWeatherRendererDX11(simul::clouds::Environment *env
													,simul::base::MemoryInterface *mem) :
	BaseWeatherRenderer(env,mem),
	framebuffer(0,0),
	m_pd3dDevice(NULL),
	m_pTonemapEffect(NULL)
	,directTechnique(NULL)
	,imageTexture(NULL)
	,worldViewProj(NULL)
	,simulSkyRenderer(NULL)
	,simulCloudRenderer(NULL)
	,simulPrecipitationRenderer(NULL)
	,simulAtmosphericsRenderer(NULL)
	,simul2DCloudRenderer(NULL)
	,simulLightningRenderer(NULL)
	,BufferWidth(0)
	,BufferHeight(0)
	,exposure_multiplier(1.f)
	,memoryInterface(mem)
{
	simul::sky::SkyKeyframer *sk=env->skyKeyframer;
	simul::clouds::CloudKeyframer *ck2d=env->cloud2DKeyframer;
	simul::clouds::CloudKeyframer *ck3d=env->cloudKeyframer;
	//if(ShowSky)
	{
		simulSkyRenderer=new(memoryInterface) SimulSkyRendererDX1x(sk,memoryInterface);
		baseSkyRenderer=simulSkyRenderer;
	}
	baseCloudRenderer=simulCloudRenderer=new(memoryInterface) SimulCloudRendererDX1x(ck3d,memoryInterface);
	
	simulLightningRenderer=new(memoryInterface) SimulLightningRendererDX11(ck3d,sk);
	if(env->cloud2DKeyframer)
		base2DCloudRenderer=simul2DCloudRenderer=new(memoryInterface) Simul2DCloudRendererDX11(ck2d,memoryInterface);
	basePrecipitationRenderer=simulPrecipitationRenderer=new(memoryInterface) SimulPrecipitationRendererDX1x();
	baseAtmosphericsRenderer=simulAtmosphericsRenderer=new(memoryInterface) SimulAtmosphericsRendererDX1x(mem);
	baseFramebuffer=&framebuffer;
	ConnectInterfaces();
}

void SimulWeatherRendererDX11::SetScreenSize(int w,int h)
{
	ScreenWidth=w;
	ScreenHeight=h;
	BufferWidth=w/Downscale;
	BufferHeight=h/Downscale;
	framebuffer.SetWidthAndHeight(BufferWidth,BufferHeight);
}

void SimulWeatherRendererDX11::RestoreDeviceObjects(void* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D1xDevice*)dev;
	framebuffer.RestoreDeviceObjects(m_pd3dDevice);

	if(simulCloudRenderer)
	{
		simulCloudRenderer->RestoreDeviceObjects(m_pd3dDevice);
		if(simulSkyRenderer)
			simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
	if(simulLightningRenderer)
		simulLightningRenderer->RestoreDeviceObjects(m_pd3dDevice);
/*	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	}*/
	if(simulSkyRenderer)
	{
		simulSkyRenderer->RestoreDeviceObjects(m_pd3dDevice);
	//	if(simulCloudRenderer)
//			simulSkyRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
	}
	//if(simulAtmosphericsRenderer&&simulSkyRenderer)
	//	simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	//V_RETURN(CreateBuffers());

	if(simul2DCloudRenderer)
		simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice);

	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
}

void SimulWeatherRendererDX11::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(m_pTonemapEffect);
	std::map<std::string,std::string> defines;
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	CreateEffect(m_pd3dDevice,&m_pTonemapEffect,("simul_hdr.fx"), defines);
	directTechnique			=m_pTonemapEffect->GetTechniqueByName("simul_direct");
	SkyBlendTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_sky_blend");
	imageTexture			=m_pTonemapEffect->GetVariableByName("imageTexture")->AsShaderResource();
	worldViewProj			=m_pTonemapEffect->GetVariableByName("worldViewProj")->AsMatrix();
	BaseWeatherRenderer::RecompileShaders();
}

void SimulWeatherRendererDX11::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pTonemapEffect);
	framebuffer.InvalidateDeviceObjects();
	if(simulSkyRenderer)
		simulSkyRenderer->InvalidateDeviceObjects();
	if(simulCloudRenderer)
		simulCloudRenderer->InvalidateDeviceObjects();
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->InvalidateDeviceObjects();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->InvalidateDeviceObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();
	if(simulLightningRenderer)
		simulLightningRenderer->InvalidateDeviceObjects();
//	if(m_pTonemapEffect)
//        hr=m_pTonemapEffect->OnLostDevice();
// Free the cubemap resources. 
}

bool SimulWeatherRendererDX11::Destroy()
{
	HRESULT hr=S_OK;
	InvalidateDeviceObjects();

	if(simulSkyRenderer)
		simulSkyRenderer->Destroy();
	simulSkyRenderer=NULL;

	if(simulCloudRenderer)
		simulCloudRenderer->Destroy();
	
	simulCloudRenderer=NULL;

	simulAtmosphericsRenderer=NULL;
	return (hr==S_OK);
}

SimulWeatherRendererDX11::~SimulWeatherRendererDX11()
{
	InvalidateDeviceObjects();
	// Free this memory here instead of global destruction(as causes shutdown problems).
	simul::dx11::UtilityRenderer::InvalidateDeviceObjects();
	Destroy();
	del(simulSkyRenderer,memoryInterface);
	del(simulCloudRenderer,memoryInterface);
	del(simulPrecipitationRenderer,memoryInterface);
	del(simulAtmosphericsRenderer,memoryInterface);
	del(simul2DCloudRenderer,memoryInterface);
	del(simulLightningRenderer,memoryInterface);
}

/*bool SimulWeatherRendererDX11::IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat)
{
	LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
    // Verify that the depth format exists
    HRESULT hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat,
                                                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the backbuffer format is valid
    hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_RENDERTARGET,
                                               D3DRTYPE_SURFACE, BackBufferFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the depth format is compatible
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat,
                                                    DepthFormat);

    return (hr==S_OK);
}

static D3DXVECTOR3 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR3 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}
*/
void SimulWeatherRendererDX11::SaveCubemapToFile(const char *filename)
{
	static float exposure=1.f;
	ID3D11DeviceContext* m_pImmediateContext=NULL;;
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	FramebufferCubemapDX1x	fb_cubemap;
	fb_cubemap.SetWidthAndHeight(2048,2048);
	fb_cubemap.RestoreDeviceObjects(m_pd3dDevice);
	simul::dx11::Framebuffer gamma_correct;
	gamma_correct.SetWidthAndHeight(2048,2048);
	gamma_correct.RestoreDeviceObjects(m_pd3dDevice);

	ID3D1xEffect* m_pTonemapEffect=NULL;
	CreateEffect(m_pd3dDevice,&m_pTonemapEffect,("simul_hdr.fx"));
	ID3D1xEffectTechnique *tech=m_pTonemapEffect->GetTechniqueByName("simul_gamma");

	cam_pos=GetCameraPosVector(view);
	D3DXMATRIX view_matrices[6];
	MakeCubeMatrices(view_matrices,cam_pos,ReverseDepth);
	bool noise3d=environment->cloudKeyframer->GetUse3DNoise();
	environment->cloudKeyframer->SetUse3DNoise(true);
	int l=100;
	if(baseCloudRenderer)
	{
		baseCloudRenderer->RecompileShaders();
		l=baseCloudRenderer->GetCloudGeometryHelper()->GetMaxLayers();
		baseCloudRenderer->GetCloudGeometryHelper()->SetMaxLayers(250);
	}
	for(int i=0;i<6;i++)
	{
		fb_cubemap.SetCurrentFace(i);
		fb_cubemap.Activate(m_pImmediateContext);
		gamma_correct.Activate(m_pImmediateContext);
		gamma_correct.Clear(m_pImmediateContext,0.f,0.f,0.f,0.f,0.f);
		if(simulSkyRenderer)
		{
			D3DXMATRIX cube_proj;
			D3DXMatrixPerspectiveFovRH(&cube_proj,
				3.1415926536f/2.f,
				1.f,
				1.f,
				600000.f);
			SetMatrices(view_matrices[i],cube_proj);
			HRESULT hr=RenderSky(m_pImmediateContext,exposure,false,true);
		}
		gamma_correct.Deactivate(m_pImmediateContext);
		{
			simul::dx11::setParameter(m_pTonemapEffect,"imageTexture",gamma_correct.GetBufferResource());
			simul::dx11::setParameter(m_pTonemapEffect,"gamma",.45f);
			simul::dx11::setParameter(m_pTonemapEffect,"exposure",1.f);
			ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
			gamma_correct.DrawQuad(m_pImmediateContext);
		}
		fb_cubemap.Deactivate(m_pImmediateContext);
	}
	ID3D11Texture2D *tex=fb_cubemap.GetCopy(m_pImmediateContext);
	HRESULT hr=D3DX11SaveTextureToFileA(m_pImmediateContext,tex,D3DX11_IFF_DDS,filename);
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(m_pTonemapEffect);
	environment->cloudKeyframer->SetUse3DNoise(noise3d);
	if(baseCloudRenderer)
	{
		baseCloudRenderer->GetCloudGeometryHelper()->SetMaxLayers(l);
	}
}

void SimulWeatherRendererDX11::RenderSkyAsOverlay(void *context,float exposure,bool buffered,bool is_cubemap,const void* depthTexture,int viewport_id,const simul::sky::float4& viewportTextureRegionXYWH)
{
	BaseWeatherRenderer::RenderSkyAsOverlay(context,exposure,buffered,is_cubemap,depthTexture,viewport_id,viewportTextureRegionXYWH);
	
//	if(baseAtmosphericsRenderer&&!is_cubemap)
//		baseAtmosphericsRenderer->RenderGodrays(context,depthTexture,exposure,viewportTextureRegionXYWH);
	if(buffered&&baseFramebuffer)
	{
		bool blend=!is_cubemap;
		imageTexture->SetResource(framebuffer.buffer_texture_SRV);
		ID3D1xEffectTechnique *tech=blend?SkyBlendTechnique:directTechnique;
		ApplyPass((ID3D11DeviceContext*)context,tech->GetPassByIndex(0));
		simul::dx11::setParameter(m_pTonemapEffect,"exposure",1.f);
		simul::dx11::setParameter(m_pTonemapEffect,"gamma",1.f);
		framebuffer.DrawQuad(context);
		imageTexture->SetResource(NULL);
	}

	if(baseAtmosphericsRenderer&&!is_cubemap)
		baseAtmosphericsRenderer->RenderGodrays(context,depthTexture,exposure,viewportTextureRegionXYWH);
}

bool SimulWeatherRendererDX11::RenderSky(void *context,float exposure,bool buffered,bool is_cubemap)
{
	BaseWeatherRenderer::RenderSky(context,exposure,buffered,is_cubemap);
	if(buffered&&baseFramebuffer)
	{
		bool blend=!is_cubemap;
		HRESULT hr=S_OK;
		hr=imageTexture->SetResource(framebuffer.buffer_texture_SRV);
		ID3D1xEffectTechnique *tech=blend?SkyBlendTechnique:directTechnique;
		ApplyPass((ID3D11DeviceContext*)context,tech->GetPassByIndex(0));
		
		D3DXMATRIX ortho;
		D3DXMatrixIdentity(&ortho);
		D3DXMatrixOrthoLH(&ortho,2.f,2.f,-100.f,100.f);
		worldViewProj->SetMatrix(ortho);
		
		framebuffer.DrawQuad(context);
		imageTexture->SetResource(NULL);
	}
	return true;
}

void SimulWeatherRendererDX11::RenderLateCloudLayer(void *context,float exposure,bool,int viewport_id,const simul::sky::float4& relativeViewportTextureRegionXYWH)
{
	if(simulCloudRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
	{
		const simul::sky::float4 viewportTextureRegionXYWH(0,0,1,1);
		simulCloudRenderer->Render(context,exposure,false,0,UseDefaultFog,true,viewport_id,viewportTextureRegionXYWH);
	}
}

void SimulWeatherRendererDX11::RenderPrecipitation(void *context)
{
	if(basePrecipitationRenderer)
		basePrecipitationRenderer->SetIntensity(environment->cloudKeyframer->GetPrecipitationIntensity(cam_pos));
	if(simulPrecipitationRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render(context);
}

void SimulWeatherRendererDX11::RenderLightning(void *context,int viewport_id)
{
	if(simulCloudRenderer&&simulLightningRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
		simulLightningRenderer->Render(context);
}

void SimulWeatherRendererDX11::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
	cam_pos=GetCameraPosVector(view);
	if(simulSkyRenderer)
		simulSkyRenderer->SetMatrices(view,proj);
	if(simulCloudRenderer)
		simulCloudRenderer->SetMatrices(view,proj);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->SetMatrices(view,proj);
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(view,proj);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(view,proj);
	if(simulLightningRenderer)
		simulLightningRenderer->SetMatrices(view,proj);
}

SimulSkyRendererDX1x *SimulWeatherRendererDX11::GetSkyRenderer()
{
	return simulSkyRenderer;
}

SimulCloudRendererDX1x *SimulWeatherRendererDX11::GetCloudRenderer()
{
	return simulCloudRenderer;
}

Simul2DCloudRendererDX11 *SimulWeatherRendererDX11::Get2DCloudRenderer()
{
	return simul2DCloudRenderer;
}
//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
void SimulWeatherRendererDX11::SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb)
{
}