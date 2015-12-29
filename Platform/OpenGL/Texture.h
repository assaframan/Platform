#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include <string>
#include <map>
#pragma warning(disable:4251)
namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT SamplerState:public crossplatform::SamplerState
		{
		public:
			GLuint sampler_state;
			SamplerState();
			virtual ~SamplerState();
			void InvalidateDeviceObjects();
			GLuint asGLuint()
			{
				return sampler_state;
			}
		};
		class SIMUL_OPENGL_EXPORT Texture:public simul::crossplatform::Texture
		{
			GLuint m_fb;
			int main_viewport[4];
			bool externalTextureObject;
		public:
			Texture();
			~Texture();
			void LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8);
			void LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files);
			bool IsValid() const;
			void InvalidateDeviceObjects();
			GLuint AsGLuint(int view);
			GLuint FramebufferAsGLuint()
			{
				return m_fb;
			}
			/// Initialize from native OpenGL texture GLuint t. srv is ignored.
			void InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,bool make_rt=false) override;
			void ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l
				,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0,bool wrap=false);
			void ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool cubemap=false) override;
			void ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat frmt,bool computable=false,int mips=1,bool rendertargets=false);
			void GenerateMips(crossplatform::DeviceContext &deviceContext);
			void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels);
			void activateRenderTarget(crossplatform::DeviceContext &deviceContext);
			void deactivateRenderTarget();
			int GetLength() const;
			int GetWidth() const;
			int GetDimension() const;
			int GetSampleCount() const;
			void copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels);
			GLuint pTextureObject;
			GLuint *pViewObjects;
			int numViews;
			// Former Texture functions
			void setTexels(void *,const void *src,int x,int y,int z,int w,int l,int d);
		};
	}
}
