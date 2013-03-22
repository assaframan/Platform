#pragma once
#include <tchar.h>
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/FramebufferCubemapDX1x.h"
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/BaseLightningRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"

class SimulLightningRendererDX11: public simul::clouds::BaseLightningRenderer
{
public:
	SimulLightningRendererDX11();
	~SimulLightningRendererDX11();
	void RestoreDeviceObjects(void* dev);
	void RecompileShaders();
	void InvalidateDeviceObjects();
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	void Render();
protected:
};