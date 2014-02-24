#ifndef TEXTRENDERER_H
#define TEXTRENDERER_H

#include <d3d11.h>
#include <d3dx10math.h>
#include <d3dx11async.h>
#include <fstream>
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
namespace simul
{
	namespace dx11
	{
		class TextRenderer
		{
		private:
			struct ConstantBufferType
			{
			vec4	rect;
			vec4  texc;
			};

		public:
			TextRenderer();
			~TextRenderer();

			void RestoreDeviceObjects(ID3D11Device*);
			void InvalidateDeviceObjects();
		void RecompileShaders();
			void Render(ID3D11DeviceContext *context, float x,float y,float screen_width,float screen_height,const char *txt);

		private:

		private:
			ID3D11Device						*m_pd3dDevice;
			ID3DX11Effect						*effect;
	
			ConstantBuffer<ConstantBufferType>	constantBuffer;
			ID3D11SamplerState* m_sampleState;
			ID3D11Buffer* m_pixelBuffer;
			ID3D1xShaderResourceView*			font_texture_SRV;
		};
	}
}
#endif