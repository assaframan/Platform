#pragma once

#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/Effect.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif

namespace simul
{
	namespace vulkan
	{
		class Material;
        class Texture;

		//! Vulkan renderplatform implementation
		class SIMUL_VULKAN_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
		public:
			            RenderPlatform();
			virtual~    RenderPlatform() override;
			const char* GetName() const override;
			void        RestoreDeviceObjects(void*) override;
			void        InvalidateDeviceObjects() override;
			void        BeginFrame() override;
			void        EndFrame() override;
            void        BeginEvent(crossplatform::DeviceContext& deviceContext, const char* name)override;
            void        EndEvent(crossplatform::DeviceContext& deviceContext)override;
			void        DispatchCompute(crossplatform::DeviceContext& deviceContext, int w, int l, int d) override;
			void        Draw(crossplatform::DeviceContext& deviceContext, int num_verts, int start_vert) override;
			void        DrawIndexed(crossplatform::DeviceContext& deviceContext, int num_indices, int start_index = 0, int base_vertex = 0) override;
			void        DrawLine(crossplatform::DeviceContext& deviceContext, const double *pGlobalBasePosition, const double *pGlobalEndPosition, const float *colour, float width);
			void        DrawLineLoop(crossplatform::DeviceContext& deviceContext, const double *mat, int num, const double *vertexArray, const float colr[4]) override;
			void        DrawTexture(crossplatform::DeviceContext& deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, vec4 mult, bool blend = false, float gamma = 1.0f, bool debug = false) override;
			void        DrawQuad(crossplatform::DeviceContext& deviceContext) override;
			void        DrawLines(crossplatform::DeviceContext& deviceContext, crossplatform::PosColourVertex* lines, int count, bool strip = false, bool test_depth = false, bool view_centred = false) override;
			void        Draw2dLines(crossplatform::DeviceContext& deviceContext, crossplatform::PosColourVertex* lines, int count, bool strip) override;
			void        DrawCircle(crossplatform::DeviceContext& context, const float *dir, float rads, const float* colr, bool fill = false) override;
            void        GenerateMips(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* t, bool wrap, int array_idx = -1)override;
            //! This should be called after a Draw/Dispatch command that uses
            //! textures. Here we will apply the textures.
            void        ApplyCurrentPass(crossplatform::DeviceContext& deviceContext);

            void        InsertFences(crossplatform::DeviceContext& deviceContext);

            crossplatform::Material*                CreateMaterial();
			crossplatform::Texture*                 CreateTexture(const char *lFileNameUtf8= nullptr) override;
			crossplatform::BaseFramebuffer*         CreateFramebuffer(const char *name=nullptr) override;
			crossplatform::SamplerState*            CreateSamplerState(crossplatform::SamplerStateDesc *) override;
			crossplatform::Effect*                  CreateEffect() override;
			crossplatform::Effect*                  CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines) override;
			crossplatform::PlatformConstantBuffer*  CreatePlatformConstantBuffer() override;
			crossplatform::PlatformStructuredBuffer*CreatePlatformStructuredBuffer() override;
			crossplatform::Buffer*                  CreateBuffer() override;
			crossplatform::RenderState*             CreateRenderState(const crossplatform::RenderStateDesc &desc) override;
			crossplatform::Query*                   CreateQuery(crossplatform::QueryType type) override;
			crossplatform::Shader*                  CreateShader() override;

			crossplatform::DisplaySurface*			CreateDisplaySurface() override;
			void*                                   GetDevice();
			void									SetVertexBuffers(crossplatform::DeviceContext &deviceContext, int slot, int num_buffers,crossplatform::Buffer *const*buffers, const crossplatform::Layout *layout, const int *vertexSteps = NULL) override;
			
			void									SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer,int start_index=0) override;
			void									ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth) override;
			void									DeactivateRenderTargets(crossplatform::DeviceContext &) override;
			void									SetViewports(crossplatform::DeviceContext &deviceContext,int num,const crossplatform::Viewport *vps) override;

			void									SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer) override;
			
			void									SetTopology(crossplatform::DeviceContext &deviceContext,crossplatform::Topology t) override;
			void									EnsureEffectIsBuilt				(const char *filename_utf8,const std::vector<crossplatform::EffectDefineOptions> &options) override;

			void									StoreRenderState(crossplatform::DeviceContext &deviceContext) override;
			void									RestoreRenderState(crossplatform::DeviceContext &deviceContext) override;
			void									PopRenderTargets(crossplatform::DeviceContext &deviceContext) override;
			void									SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s) override;
            void					                SetStandardRenderState(crossplatform::DeviceContext& deviceContext, crossplatform::StandardRenderState s)override;
			void									Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source) override;
			void									SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8) override;
			
        /*    static Vulkanenum                           toVulkanTopology(crossplatform::Topology t);
            static Vulkanenum                           toVulkanMinFiltering(crossplatform::SamplerStateDesc::Filtering f);
            static Vulkanenum                           toVulkanMaxFiltering(crossplatform::SamplerStateDesc::Filtering f);
			static Vulkanint                           toVulkanWrapping(crossplatform::SamplerStateDesc::Wrapping w);
			static                                  Vulkanuint ToVulkanFormat(crossplatform::PixelFormat p);
			static                                  crossplatform::PixelFormat FromVulkanFormat(Vulkanuint p);
			static                                  Vulkanuint ToVulkanExternalFormat(crossplatform::PixelFormat p);
			static                                  Vulkanenum DataType(crossplatform::PixelFormat p);*/
			static int                              FormatCount(crossplatform::PixelFormat p);
            
            //! Makes the handle resident only if its not resident already
           // void                                    MakeTextureResident(Vulkanuint64 handle);
            //! Returns 2D dummy texture 1 white texel
            vulkan::Texture*                        GetDummy2D();
            //! Returns 3D dummy texture 1 white texel
            vulkan::Texture*                        GetDummy3D();

        private:

            vulkan::Texture*    mDummy2D;
            vulkan::Texture*    mDummy3D;
		};
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
