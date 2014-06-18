#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
struct ID3D11ShaderResourceView;
typedef unsigned GLuint;
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		public:
			Texture()
				:width(0)
				,length(0)
				,dim(0){}
			virtual ~Texture();
			virtual void LoadFromFile(RenderPlatform *r,const char *pFilePathUtf8)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual ID3D11ShaderResourceView *AsD3D11ShaderResourceView()=0;
			virtual GLuint AsGLuint()=0;
			virtual void ensureTexture2DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l
				,unsigned f,bool computable=false,bool rendertarget=false,int num_samples=1,int aa_quality=0)=0;
			virtual void activateRenderTarget(DeviceContext &deviceContext)=0;
			virtual void deactivateRenderTarget()=0;
			virtual int GetLength() const=0;
			virtual int GetWidth() const=0;
			virtual int GetDimension() const=0;
			//! If the texture is multisampled, this returns the samples per texel. Zero means it is not an MS texture,
			//! while 1 means it is MS, even though the sample count is unity.
			virtual int GetSampleCount() const=0;
			
			int width,length,depth,dim;
		};
	}
}