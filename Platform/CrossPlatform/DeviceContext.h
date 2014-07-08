#ifndef SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#define SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#include "BaseRenderer.h"
struct ID3D11DeviceContext;
struct IDirect3DDevice9;
namespace sce
{
	namespace Gnmx
	{
		class GfxContext;
	}
}
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		//! The base class for Device contexts. The actual context pointer is only applicable in DirectX - in OpenGL, it will be null.
		//! The DeviceContext also carries a pointer to the platform-specific RenderPlatform.
		//! DeviceContext is context in the DirectX11 sense, encompassing a platform-specific deviceContext pointer
		struct DeviceContext
		{
			void *platform_context;
			RenderPlatform *renderPlatform;
			DeviceContext():
				platform_context(NULL)
				,renderPlatform(NULL)
			{
				viewStruct.depthTextureStyle=crossplatform::PROJECTION;
			}
			inline ID3D11DeviceContext *asD3D11DeviceContext()
			{
				return (ID3D11DeviceContext*)platform_context;
			}
			inline IDirect3DDevice9 *asD3D9Device()
			{
				return (IDirect3DDevice9*)platform_context;
			}
			inline sce::Gnmx::GfxContext *asGfxContext()
			{
				return (sce::Gnmx::GfxContext*)platform_context;
			}
			ViewStruct viewStruct;
		};
	}
}
#endif