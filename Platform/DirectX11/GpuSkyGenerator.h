#pragma once
#include "Simul/Sky/BaseGpuSkyGenerator.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Texture.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#endif
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#endif

namespace simul
{
	namespace dx11
	{
		class GpuSkyGenerator:public simul::sky::BaseGpuSkyGenerator
		{
		public:
			GpuSkyGenerator();
			~GpuSkyGenerator();
		// Implementing BaseGpuSkyGenerator
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			//! Return true if the derived class can make sky tables using the GPU.
			bool CanPerformGPUGeneration() const;
			void MakeLossAndInscatterTextures(sky::float4 wavelengthsNm,int cycled_index,
				simul::sky::AtmosphericScatteringInterface *skyInterface
				,const sky::GpuSkyParameters &gpuSkyParameters
				,const sky::GpuSkyAtmosphereParameters &gpuSkyAtmosphereParameters
				,const sky::GpuSkyInfraredParameters &gpuSkyInfraredParameters);
			void CopyToMemory(int cycled_index,simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl);
			// If we want the generator to put the data directly into 3d textures:
			void SetDirectTargets(crossplatform::Texture **loss,crossplatform::Texture **insc,crossplatform::Texture **skyl,crossplatform::Texture *light_table)
			{
				for(int i=0;i<3;i++)
				{
					if(loss)
						finalLoss[i]=loss[i];
					else
						finalLoss[i]=NULL;
					if(insc)
						finalInsc[i]=insc[i];
					else
						finalInsc[i]=NULL;
					if(skyl)
						finalSkyl[i]=skyl[i];
					else
						finalSkyl[i]=NULL;
					this->light_table=light_table;
				}
			}
		protected:
			ID3D11Device*					m_pd3dDevice;
			ID3D11DeviceContext*			m_pImmediateContext;
			ID3DX11Effect*					effect;
			ID3DX11EffectTechnique*			lossComputeTechnique;
			ID3DX11EffectTechnique*			inscComputeTechnique;
			ID3DX11EffectTechnique*			skylComputeTechnique;
			ID3DX11EffectTechnique*			lightComputeTechnique;
			
			ID3D1xBuffer*					constantBuffer;
			ConstantBuffer<GpuSkyConstants>	gpuSkyConstants;
			crossplatform::Texture			*finalLoss[3];
			crossplatform::Texture			*finalInsc[3];
			crossplatform::Texture			*finalSkyl[3];
			crossplatform::Texture			*light_table;
			dx11::Texture					dens_tex,optd_tex;

			unsigned						tables_checksum;
		};
	}
}