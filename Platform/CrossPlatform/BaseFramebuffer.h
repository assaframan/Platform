#ifndef BASE_FRAMEBUFFER_H
#define BASE_FRAMEBUFFER_H
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/spherical_harmonics_constants.sl"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		class Texture;
		struct DeviceContext;
		class RenderPlatform;
		//! A base class for API-dependent framebuffer classes.
		SIMUL_CROSSPLATFORM_EXPORT_CLASS BaseFramebuffer
		{
		public:
			BaseFramebuffer(int w=0,int h=0);
			virtual ~BaseFramebuffer(){};
			//! Call this when the API-dependent device has been created.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			//! Call this when the API-dependent device has been lost or is shutting down.
			virtual void InvalidateDeviceObjects();
			/// Return true if the API-dependent objects have been updated to match the properties.
			virtual bool IsValid() const;
			virtual void SetExternalTextures(crossplatform::Texture *colour,crossplatform::Texture *depth);
			//! Call this if needed (not usually) to ensure that the buffers are created.
			virtual bool CreateBuffers();
			//! Activate the framebuffer and set the viewport- must be followed after rendering by a call to \ref Deactivate().
			virtual void ActivateViewport(crossplatform::DeviceContext &,float viewportX, float viewportY, float viewportW, float viewportH)=0;
			//! Activate the framebuffer - must be followed after rendering by a call to \ref Deactivate().
			virtual void Activate(crossplatform::DeviceContext &)=0;
			//! Activate the colour part of this framebuffer, without depth - must be followed after rendering by a call to \ref Deactivate().
			virtual void ActivateColour(crossplatform::DeviceContext &,const float viewportXYWH[4])=0;
			virtual void ActivateDepth(crossplatform::DeviceContext &)=0;
			//! Return true if the framebuffer's depth buffer has been activated and not yet deactivated.
			virtual bool IsDepthActive() const;
			//! Return true if the framebuffer's colour buffer has been activated and not yet deactivated.
			virtual bool IsColourActive() const;
			//! Deactivate the framebuffer - must be preceded a call to \ref Activate().
			virtual void Deactivate(DeviceContext &)=0;
			//! Deactivate only the depth buffer, so it can be used as a texture for rendering to the colour buffer.
			virtual void DeactivateDepth(DeviceContext &){}
			//! Set the API-dependent colour buffer format for this framebuffer. Across all API's, setting 0 means no rendering to colour.
			virtual void SetFormat(PixelFormat);
			//! Set the API-dependent colour depth format for this framebuffer. Across all API's, setting 0 means no rendering to depth.
			virtual void SetDepthFormat(PixelFormat);
			virtual void SetGenerateMips(bool);
			//! Clear the colour and depth buffers if present.
			virtual void Clear(crossplatform::DeviceContext &context,float R,float G,float B,float A,float depth,int mask=0)=0;
			//! Set the size of the framebuffer in pixel height and width.
			virtual void ClearColour(crossplatform::DeviceContext &context,float,float,float,float)=0;
			//! Set the size of the framebuffer.
			virtual void SetWidthAndHeight(int w,int h);
			//! Set this to be a cubemap framebuffer, so that its texture object will be a cubemap. Equivalent to SetWidthAndHeight.
			virtual void SetAsCubemap(int face_size);
			virtual void SetCubeFace(int f);
			//! Some hardware has fast RAM that's good for framebuffers.
			virtual void SetUseFastRAM(bool /*colour*/,bool /*depth*/){};
			virtual void SetAntialiasing(int s)=0;
			//! Calculate the spherical harmonics of this cubemap and store the result internally.
			//! Changing the number of bands will resize the internal storeage.
			virtual void CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext);
			virtual void RecompileShaders();
			//! Get the texture for the colour buffer target.
			inline Texture *GetTexture()
			{
				return buffer_texture;
			}
			//! Get the texture for the depth buffer target.
			inline Texture *GetDepthTexture()
			{
				return buffer_depth_texture;
			}
			//! Get the dimension of the surface
			inline int GetWidth()
			{
				return Width;
			}
			//! Get the dimension of the surface
			inline int GetHeight()
			{
				return Height;
			}
		//protected:
			//! The size of the buffer
			int Width,Height;
			int numAntialiasingSamples;
			bool depth_active, colour_active;
			crossplatform::RenderPlatform *renderPlatform;
			crossplatform::StructuredBuffer<vec4> &GetSphericalHarmonics()
			{
				return sphericalHarmonics;
			}
			void SetBands(int b)
			{
				if(b>MAX_SH_BANDS)
					b=MAX_SH_BANDS;
				if(bands!=b)
				{
					bands=b;
					sphericalHarmonics.InvalidateDeviceObjects();
				}
			}
		protected:
			//! The depth buffer.
			crossplatform::Texture				*buffer_texture;
			crossplatform::Texture				*buffer_depth_texture;
			bool GenerateMips;
			bool is_cubemap;
			int	current_face;
			crossplatform::PixelFormat target_format;
			crossplatform::PixelFormat depth_format;
			int bands;
			int activate_count;

			crossplatform::ConstantBuffer<SphericalHarmonicsConstants>	sphericalHarmonicsConstants;
			crossplatform::StructuredBuffer<SphericalHarmonicsSample>	sphericalSamples;
			crossplatform::StructuredBuffer<vec4>						sphericalHarmonics;
			crossplatform::Effect										*sphericalHarmonicsEffect;
			bool external_texture;
			bool external_depth_texture;
		};
		class SIMUL_CROSSPLATFORM_EXPORT CubemapFramebuffer:public BaseFramebuffer
		{
			virtual void SetCurrentFace(int i)=0;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif