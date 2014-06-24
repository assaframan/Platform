#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/SL/Cppsl.hs"
#include "Simul/Platform/CrossPlatform/SL/solid_constants.sl"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/DirectX11/Utilities.h"

#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		class Material;
		class ConstantBufferBase;
	}
	namespace dx11
	{
		class ConstantBufferCache;
		class Material;
		//! A class to implement common rendering functionality for DirectX 11.
		class SIMUL_DIRECTX11_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
			ID3D11Device *device;
		public:
			RenderPlatform();
			virtual ~RenderPlatform();
			///  The \em platform_device parameter should be the ID3D11Device pointer.
			void RestoreDeviceObjects(void *platform_device);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			
			ID3D11Device *AsD3D11Device()
			{
				return device;
			}
			void PushTexturePath(const char *pathUtf8);
			void PopTexturePath();
			void StartRender();
			void EndRender();
			void SetReverseDepth(bool r);
			void IntializeLightingEnvironment(const float pAmbientLight[3]);

			void DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d);
			
			void ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *,crossplatform::EffectTechnique *,int index);
			
			void Draw			(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert);
			void DrawMarker		(void *context,const double *matrix);
			void DrawLine		(void *context,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width);
			void DrawCrossHair	(void *context,const double *pGlobalPosition);
			void DrawCamera		(void *context,const double *pGlobalPosition, double pRoll);
			void DrawLineLoop	(void *context,const double *mat,int num,const double *vertexArray,const float colr[4]);
			void DrawTexture	(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult=1.f);
			void DrawDepth		(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex);
			void DrawQuad		(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique);
			void DrawQuad		(crossplatform::DeviceContext &deviceContext);

			void Print			(crossplatform::DeviceContext &deviceContext,int x,int y	,const char *text);
			void DrawLines		(crossplatform::DeviceContext &deviceContext,Vertext *lines,int count,bool strip=false);
			void Draw2dLines	(crossplatform::DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip);
			void DrawCircle		(crossplatform::DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill=false);
			void PrintAt3dPos	(void *context,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);

			void ApplyDefaultMaterial();
			void SetModelMatrix(crossplatform::DeviceContext &deviceContext,const double *mat);
			crossplatform::Material					*CreateMaterial();
			crossplatform::Mesh						*CreateMesh();
			crossplatform::Light					*CreateLight();
			crossplatform::Texture					*CreateTexture(const char *lFileNameUtf8);
			crossplatform::Effect					*CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines);
			crossplatform::PlatformConstantBuffer	*CreatePlatformConstantBuffer();
			crossplatform::Buffer					*CreateBuffer();
			crossplatform::Layout					*CreateLayout(int num_elements,crossplatform::LayoutDesc *,crossplatform::Buffer *);
			void									*GetDevice();
			void									SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers,crossplatform::Buffer **buffers);
			ID3DX11Effect *effect;
			simul::dx11::ConstantBuffer<SolidConstants> solidConstants;
			std::set<crossplatform::Material*> materials;
			bool reverseDepth;
			//! This was introduced because Unity's deferred renderer flips the image vertically sometime after we render.
			bool mirrorY;
			// DX11-specific stuff:
			static DXGI_FORMAT ToDxgiFormat(crossplatform::PixelFormat p);
			static crossplatform::PixelFormat FromDxgiFormat(DXGI_FORMAT f);
		protected:
			void DrawTexture	(void *context,int x1,int y1,int dx,int dy,ID3D11ShaderResourceView *tex,float mult);
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif