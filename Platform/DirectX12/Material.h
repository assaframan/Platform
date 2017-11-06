#pragma once

#include <map>

#include "Export.h"
#include "Simul/Platform/CrossPlatform/Material.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
namespace simul
{
	namespace dx12
	{
		class SIMUL_DIRECTX12_EXPORT Material:public crossplatform::Material
		{
		public:
			Material();
			virtual ~Material();
			void Apply(crossplatform::DeviceContext &,crossplatform::PhysicalLightRenderData &physicalLightRenderData) ;
		};
	}
}