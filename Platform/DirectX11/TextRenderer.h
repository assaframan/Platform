#ifndef TEXTRENDERER_H
#define TEXTRENDERER_H

#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx10math.h>
#include <d3dx11async.h>
#endif
#include <fstream>
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;
	}
	namespace dx11
	{
		class TextRenderer
		{
		private:
			struct ConstantBufferType
			{
				vec4	rect;
				vec4	texc;
				vec4	colour;
				vec4	background;
			};

		public:
			TextRenderer();
			~TextRenderer();

			void RestoreDeviceObjects(ID3D11Device*);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			//void Render(crossplatform::DeviceContext &deviceContext, float x,float y,float screen_width,float screen_height,const char *txt,const float *clr=NULL);
			void Render(crossplatform::DeviceContext &deviceContext, float x,float y,float screen_width,float screen_height,const char *txt,const float *clr=NULL,const float *bck=NULL,bool mirrorY=false);

		private:

		private:
			ID3D11Device						*m_pd3dDevice;
			ID3DX11Effect						*effect;
	
			ConstantBuffer<ConstantBufferType>	constantBuffer;
			ID3D11SamplerState* m_sampleState;
			ID3D11Buffer* m_pixelBuffer;
			ID3D11ShaderResourceView*			font_texture_SRV;
		};
	}
}
#endif