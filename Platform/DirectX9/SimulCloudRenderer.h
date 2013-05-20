
// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulCloudRenderer.h A DirectX renderer for clouds. Create an instance of this class in a DX program
// and use the Render() function once per frame.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Base/SmartPtr.h"
#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Graph/Meta/Resource.h"
#include "Simul/Graph/StandardNodes/ShowProgressInterface.h"
#include "Simul/Platform/DirectX9/Export.h"
//#include "Simul/Platform/DirectX9/GpuCloudGenerator.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace sound
	{
		namespace fmod
		{
			class NodeSound;
		}
	}
}
typedef long HRESULT;
//! A cloud rendering class. Create an instance of this class within a DirectX program,
//! or use SimulWeatherRenderer to manage cloud and sky rendering together.
SIMUL_DIRECTX9_EXPORT_CLASS SimulCloudRenderer
	: public simul::clouds::BaseCloudRenderer
	,public simul::graph::meta::ResourceUser<simul::graph::standardnodes::ShowProgressInterface>
{
public:
	SimulCloudRenderer(simul::clouds::CloudKeyframer *ck);
	virtual ~SimulCloudRenderer();
	//GpuCloudGenerator gpuCloudGenerator;
	META_BeginProperties
		META_ValueRangePropertyWithSetCall(int,NumBuffers,1,2,NumBuffersChanged,"Number of buffers to use in cloud rendering. More = faster.")
	META_EndProperties
	void RecompileShaders();
	//! Call this when the device has been created
	void RestoreDeviceObjects(void *pd3dDevice);
	//! Call this when the 3D device has been lost.
	void InvalidateDeviceObjects();
	//! DX9 implementation of cloud rendering. For this platform, depth_testing and default_fog are ignored.
	bool Render(void *context,bool cubemap,void *depth_alpha_tex,bool default_fog,bool write_alpha);
	//! Call this to draw the clouds, including any illumination by lightning.
	bool Render(void *context,bool cubemap,void *depth_alpha_tex,bool default_fog,int buffer_index,bool write_alpha);
	void *GetCloudShadowTexture();
	//! Save the first keyframe texture into a 2D image file by stacking X-Z slices vertically.
	void SaveCloudTexture(const char *filename);
	//! Draw clouds as horizontal layers
	bool RenderHorizontal(bool cubemap);
#if defined(XBOX) || defined(DOXYGEN)
	//! Call this once per frame to set the matrices (X360 only).
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
#endif
	const char *GetDebugText() const;
	float GetTiming() const;
	void *GetIlluminationTexture();
	void SetLossTexture(void *t1);
	void SetInscatterTextures(void *i,void *s);
	LPDIRECT3DTEXTURE9 GetNoiseTexture()
	{
		return noise_texture;
	}
	void RenderCrossSections(void *,int width,int height);
	bool RenderDistances(int width,int height);
	bool RenderLightVolume();
	void EnableFilter(bool f);
	void SetYVertical(bool y);
	bool IsYVertical() const{return y_vertical;}

protected:
	virtual void DrawLines(void*,VertexXyzRgba *,int ,bool ){}
	// Make up to date with respect to keyframer:
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate(void*);
	void EnsureCorrectIlluminationTextureSizes();
	void EnsureIlluminationTexturesAreUpToDate();
	void EnsureTextureCycle();

	void NumBuffersChanged();
	bool y_vertical;
	void InternalRenderHorizontal(int buffer_index=0);
	void InternalRenderRaytrace(int buffer_index=0);
	void InternalRenderVolumetric(int buffer_index=0);
	bool wrap;
	struct float2
	{
		float x,y;
		void operator=(const float*f)
		{
			x=f[0];
			y=f[1];
		}
	};
	struct float3
	{
		float x,y,z;
		void operator=(const float*f)
		{
			x=f[0];
			y=f[1];
			z=f[2];
		}
	};
	struct PosVert_t
	{
		float3 position;
	};
	struct Vertex_t
	{
		float3 position;
		float3 texCoords;
		float layerFade;
		float2 texCoordsNoise;
		float3 sunlightColour;
	};
	struct CPUFadeVertex_t : public Vertex_t
	{
		float3 loss;
		float3 inscatter;
	};
	Vertex_t *vertices;
	CPUFadeVertex_t *cpu_fade_vertices;
	bool RenderNoiseTexture();
	simul::sound::fmod::NodeSound *sound;
	float timing;

	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	m_pVtxDecl;
	LPDIRECT3DVERTEXDECLARATION9	m_pHudVertexDecl;

	LPD3DXEFFECT					m_pCloudEffect;
	D3DXHANDLE						m_hTechniqueCloud;		// Handle to technique in the effect
	D3DXHANDLE						m_hTechniqueCloudMask;		// Handle to technique in the effect
	D3DXHANDLE						m_hTechniqueCloudsAndLightning;	
	D3DXHANDLE						m_hTechniqueRaytraceWithLightning;	
	D3DXHANDLE						m_hTechniqueCrossSectionXZ;	
	D3DXHANDLE						m_hTechniqueCrossSectionXY;	
	D3DXHANDLE						m_hTechniqueRenderTo2DForSaving;
	D3DXHANDLE						m_hTechniqueColourLines;	
	

	D3DXHANDLE					worldViewProj;
	D3DXHANDLE					eyePosition;
	D3DXHANDLE					maxFadeDistanceMetres;
	D3DXHANDLE					lightResponse;
	D3DXHANDLE					crossSectionOffset;
	D3DXHANDLE					lightDir;
	D3DXHANDLE					skylightColour;
	D3DXHANDLE					fractalScale;
	D3DXHANDLE 					interp;
	D3DXHANDLE 					large_texcoords_scale;
	D3DXHANDLE 					mieRayleighRatio;
	D3DXHANDLE 					hazeEccentricity;
	D3DXHANDLE					cloudEccentricity;
	D3DXHANDLE					fadeInterp;
	D3DXHANDLE					distance;
	D3DXHANDLE					layerFade;
	D3DXHANDLE					alphaSharpness;
						
	D3DXHANDLE					lightningMultipliers;
	D3DXHANDLE					lightningColour;
	D3DXHANDLE					illuminationOrigin;
	D3DXHANDLE					illuminationScales;

	D3DXHANDLE					cloudDensity1;
	D3DXHANDLE					cloudDensity2;
	D3DXHANDLE					noiseTexture;
	D3DXHANDLE					largeScaleCloudTexture;
	D3DXHANDLE					lightningIlluminationTexture;
	D3DXHANDLE					skyLossTexture;
	D3DXHANDLE					skyInscatterTexture;
	D3DXHANDLE					skylightTexture;
	
	D3DXHANDLE					invViewProj;
	D3DXHANDLE					noiseMatrix;
	D3DXHANDLE					fractalRepeatLength;
	D3DXHANDLE					cloudScales;
	D3DXHANDLE					cloudOffset;
	D3DXHANDLE					raytraceLayerTexture;

	LPDIRECT3DVOLUMETEXTURE9	cloud_textures[3];
	LPDIRECT3DVOLUMETEXTURE9	illumination_texture;
	LPDIRECT3DTEXTURE9			noise_texture;
	LPDIRECT3DTEXTURE9			raytrace_layer_texture;
	LPDIRECT3DBASETEXTURE9		sky_loss_texture;
	LPDIRECT3DBASETEXTURE9		sky_inscatter_texture;
	LPDIRECT3DBASETEXTURE9		skylight_texture;
	LPDIRECT3DCUBETEXTURE9		cloud_cubemap;
	D3DXVECTOR4					lightning_colour;
	D3DXMATRIX					world,view,proj;
	LPDIRECT3DVERTEXBUFFER9		unitSphereVertexBuffer;
	LPDIRECT3DINDEXBUFFER9		unitSphereIndexBuffer;
	virtual bool CreateNoiseTexture(void *);
	bool MakeCubemap(void *context); // not ready yet
	//! Once per frame, fill this 1-D texture with information on the layer distances and noise offsets
	bool FillRaytraceLayerTexture();
	float last_time;
};
#ifdef _MSC_VER
	#pragma warning(pop)
#endif