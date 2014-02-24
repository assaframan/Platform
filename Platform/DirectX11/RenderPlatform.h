#pragma once
#include "Export.h"
#include "Simul/Scene/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Cppsl.hs"
#include "Simul/Platform/CrossPlatform/solid_constants.sl"
#include "Simul/Platform/DirectX11/Utilities.h"

#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx11effect.h>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT RenderPlatform:public scene::RenderPlatform
		{
			ID3D11Device*				device;
		public:
			RenderPlatform();
			virtual ~RenderPlatform();
			void RestoreDeviceObjects(void*);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			
			void PushTexturePath(const char *pathUtf8);
			void PopTexturePath();
			void StartRender();
			void EndRender();
			void IntializeLightingEnvironment(const float pAmbientLight[3]);
			void DrawMarker		(void *context,const double *matrix);
			void DrawLine		(void *context,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width);
			void DrawCrossHair	(void *context,const double *pGlobalPosition);
			void DrawCamera		(void *context,const double *pGlobalPosition, double pRoll);
			void DrawLineLoop	(void *context,const double *mat,int num,const double *vertexArray,const float colr[4]);
			void ApplyDefaultMaterial();
			void SetModelMatrix(void *context,const double *mat);
			scene::MaterialCache *CreateMaterial();
			scene::Mesh *CreateMesh();
			scene::LightCache *CreateLight();
			scene::Texture *CreateTexture(const char *lFileNameUtf8);
			
			ID3DX11Effect *effect;
			simul::dx11::ConstantBuffer<SolidConstants> solidConstants;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif