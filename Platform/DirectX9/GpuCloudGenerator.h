#pragma once
#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Platform/DirectX9/Framebuffer.h"
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
namespace simul
{
namespace dx9
{
}
}