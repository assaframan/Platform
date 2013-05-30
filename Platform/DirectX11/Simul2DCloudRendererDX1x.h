// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// Simul2DCloudRendererDX11.h A renderer for 2D cloud layers.

#pragma once
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#include "Simul/Base/SmartPtr.h"
#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"

//! A renderer for 2D cloud layers, e.g. cirrus clouds.
class Simul2DCloudRendererDX11: public simul::clouds::Base2DCloudRenderer
{
public:
	Simul2DCloudRendererDX11(simul::clouds::CloudKeyframer *ck2d);
	virtual ~Simul2DCloudRendererDX11();
	void RestoreDeviceObjects(void*);
	void RecompileShaders();
	void InvalidateDeviceObjects();
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	void Update(void *context);
	bool Render(void *context,bool cubemap,const void *depth_tex,bool default_fog,bool write_alpha);
	void RenderCrossSections(void *context,int width,int height);
	void SetLossTexture(void *l);
	void SetInscatterTextures(void *i,void *s);
	void SetWindVelocity(float x,float y);
	//
	void *GetCloudShadowTexture(){return NULL;}
protected:
	virtual void DrawLines(void *context,VertexXyzRgba *vertices,int vertex_count,bool strip){}
	void EnsureCorrectTextureSizes();
	virtual void EnsureTexturesAreUpToDate(void *context);
	void EnsureTextureCycle();
	void EnsureCorrectIlluminationTextureSizes(){}
	void EnsureIlluminationTexturesAreUpToDate(){}
	virtual bool CreateNoiseTexture(void *context){return true;}
	D3DXMATRIX				view,proj;
	ID3D11Device*			m_pd3dDevice;
	ID3DX11Effect*			effect;
	ID3DX11EffectTechnique*	tech;
	ID3D11Buffer*			vertexBuffer;
	ID3D11Buffer*			indexBuffer;
	ID3D11InputLayout*		inputLayout;
	
	ID3D11Buffer*			constantBuffer;
	int num_indices;
	
	ID3D1xShaderResourceView*				skyLossTexture_SRV;
	ID3D1xShaderResourceView*				skyInscatterTexture_SRV;
	ID3D1xShaderResourceView*				skylightTexture_SRV;

	simul::dx11::TextureStruct	coverage_tex[3];
	FramebufferDX1x			detail_fb;
};