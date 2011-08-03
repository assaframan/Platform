// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulSkyRendererDX1x.h A renderer for skies.

#pragma once
#include "Simul/Base/SmartPtr.h"
#include "Simul/Sky/SkyTexturesCallback.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include <d3dx9.h>
#ifdef DX10
	#include <d3d10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
	#include <d3dx11effect.h>
#endif
#include "Simul/Platform/DirectX1x/MacrosDX1x.h"
#include "Simul/Platform/DirectX1x/Export.h"
#include "Simul/Platform/DirectX1x/FramebufferDX1x.h"

namespace simul
{
	namespace sky
	{
		class SkyInterface;
		class Sky;
		class SkyKeyframer;
	}
}

typedef long HRESULT;

//! A renderer for skies, this class will manage an instance of simul::sky::SkyNode and use it to calculate sky colours
//! in real time for the simul_sky.fx shader.
SIMUL_DIRECTX1x_EXPORT_CLASS SimulSkyRendererDX1x:public simul::sky::BaseSkyRenderer
{
public:
	SimulSkyRendererDX1x();
	virtual ~SimulSkyRendererDX1x();

	//standard d3d object interface functions
	void ReloadShaders();
	//! Call this when the D3D device has been created or reset.
	bool RestoreDeviceObjects(ID3D1xDevice* pd3dDevice);
	//! Call this when the D3D device has been shut down.
	bool InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	bool Destroy();
	//! Call this to draw the sky, usually to the SimulWeatherRenderer's render target.
	bool Render(bool blend);
	bool RenderPointStars(){return false;}
	virtual bool RenderPlanet(void* tex,float rad,const float *dir,const float *colr,bool do_lighting);
	//! Call this to draw the sun flare, usually drawn last, on the main render target.
	bool RenderFlare(float exposure);
	bool Render2DFades();
	//! Get a value, from zero to one, which represents how much of the sun is visible.
	//! Call this when the current rendering surface is the one that has obscuring
	//! objects like mountains etc. in it, and make sure these have already been drawn.
	//! GetSunOcclusion executes a pseudo-render of an invisible billboard, then
	//! uses a hardware occlusion query to see how many pixels have passed the z-test.
	float CalcSunOcclusion(float cloud_occlusion=0.f);
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);

	void Get3DLossAndInscatterTextures(void* *l1,void* *l2,void* *i1,void* *i2);
	void Get2DLossAndInscatterTextures(void* *l1,void* *i1);

	float GetFadeInterp() const;
	void SetStepsPerDay(unsigned steps);
//! Implement the SkyTexturesCallback
	void SetSkyTextureSize(unsigned size);
	void SetFadeTextureSize(unsigned width,unsigned height,unsigned num_altitudes);
	void FillSkyTexture(int alt_index,int texture_index,int texel_index,int num_texels,const float *float4_array);
	void FillFadeTexturesSequentially(int texture_index,int texel_index,int num_texels,
						const float *loss_float4_array,
						const float *inscatter_float4_array);
	void FillFadeTextureBlocks(int ,int ,int ,int ,int ,int ,int ,const float *,const float *){}
	void CycleTexturesForward();
	const char *GetDebugText() const;
	void SetYVertical(bool y);

	// for testing:
	void DrawCubemap(ID3D1xShaderResourceView*		m_pCubeEnvMapSRV);
protected:
	bool y_vertical;
	int cycle;
	bool IsYVertical(){return y_vertical;}
	float sun_occlusion;

	void CreateFadeTextures();
	ID3D1xDevice*							m_pd3dDevice;
	ID3D1xDeviceContext *					m_pImmediateContext;
	ID3D1xBuffer*							m_pVertexBuffer;
	ID3D1xInputLayout*						m_pVtxDecl;
	ID3D1xEffect*							m_pSkyEffect;
	ID3D1xQuery*							d3dQuery;

	ID3D1xEffectMatrixVariable*			worldViewProj;
	ID3D1xEffectTechnique*				m_hTechniqueSky;
	ID3D1xEffectTechnique*				m_hTechniqueSky_CUBEMAP;
	ID3D1xEffectTechnique*				m_hTechniqueFade3DTo2D;
	ID3D1xEffectTechnique*				m_hTechniqueSun;
	ID3D1xEffectTechnique*				m_hTechniqueQuery;
	ID3D1xEffectTechnique*				m_hTechniqueFlare;
	ID3D1xEffectTechnique*				m_hTechniquePlanet;
	ID3D1xEffectVectorVariable*			lightDirection;
	ID3D1xEffectVectorVariable*			mieRayleighRatio;
	ID3D1xEffectScalarVariable*			hazeEccentricity;
	ID3D1xEffectScalarVariable*			overcastFactor;
	ID3D1xEffectScalarVariable*			skyInterp;
	ID3D1xEffectScalarVariable*			altitudeTexCoord;
	ID3D1xEffectVectorVariable*			sunlightColour;

	ID3D1xEffectMatrixVariable*			projMatrix;
	ID3D1xEffectMatrixVariable*			cubemapViews;

	ID3D1xEffectShaderResourceVariable*	flareTexture;
	ID3D1xEffectShaderResourceVariable*	skyTexture1;
	ID3D1xEffectShaderResourceVariable*	skyTexture2;
	ID3D1xEffectShaderResourceVariable*	fadeTexture1;
	ID3D1xEffectShaderResourceVariable*	fadeTexture2;

	ID3D1xTexture2D*					flare_texture;
	ID3D1xTexture2D*					sky_textures[3];

	ID3D1xTexture3D*					loss_textures[3];
	ID3D1xTexture3D*					inscatter_textures[3];

	// Small framebuffers we render to once per frame to perform fade interpolation.
	FramebufferDX1x*					loss_2d;
	FramebufferDX1x*					inscatter_2d;

	ID3D1xShaderResourceView*			flare_texture_SRV;
	ID3D1xShaderResourceView*			sky_textures_SRV[3];
	ID3D1xShaderResourceView*			loss_textures_SRV[3];
	ID3D1xShaderResourceView*			insc_textures_SRV[3];

	int mapped_sky;
	D3D1x_MAPPED_TEXTURE2D sky_texture_mapped;
	int mapped_fade;
	D3D1x_MAPPED_TEXTURE3D loss_texture_mapped;
	D3D1x_MAPPED_TEXTURE3D insc_texture_mapped;

	void MapFade(int s);
	void UnmapFade();
	void MapSky(int s);
	void UnmapSky();
	D3DXMATRIX				world,view,proj;
	bool UpdateSkyTexture(float proportion);
	bool CreateSkyTexture();
	bool RenderAngledQuad(D3DXVECTOR4 dir,float half_angle_radians);
	bool RenderSun();
void DrawCube();
};