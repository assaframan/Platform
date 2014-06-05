#define NOMINMAX
#include "Effect.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Texture.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"

#include <string>

using namespace simul;
using namespace dx11;
void dx11::PlatformConstantBuffer::RestoreDeviceObjects(void *dev,size_t size,void *addr)
{
	InvalidateDeviceObjects();
	SAFE_RELEASE(m_pD3D11Buffer);	
	D3D11_SUBRESOURCE_DATA cb_init_data;
	cb_init_data.pSysMem			= addr;
	cb_init_data.SysMemPitch		= 0;
	cb_init_data.SysMemSlicePitch	= 0;
	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage				= D3D11_USAGE_DYNAMIC;
	cb_desc.BindFlags			= D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags		= D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags			= 0;
	cb_desc.ByteWidth			= PAD16(size);
	cb_desc.StructureByteStride = 0;
	ID3D11Device *device=(ID3D11Device*)dev;
	device->CreateBuffer(&cb_desc,&cb_init_data, &m_pD3D11Buffer);
	if(m_pD3DX11EffectConstantBuffer)
		m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
}
//! Find the constant buffer in the given effect, and link to it.
void dx11::PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name)
{
	m_pD3DX11EffectConstantBuffer=effect->asD3DX11Effect()->GetConstantBufferByName(name);
	if(m_pD3DX11EffectConstantBuffer)
		m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
	else
		std::cerr<<"ConstantBuffer<> LinkToEffect did not find the buffer named "<<name<<" in the effect."<<std::endl;
}

void dx11::PlatformConstantBuffer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pD3D11Buffer);
	m_pD3DX11EffectConstantBuffer=NULL;
}

void dx11::PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.platform_context;
	
	D3D11_MAPPED_SUBRESOURCE mapped_res;
	if(pContext->Map(m_pD3D11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res)!=S_OK)
		return;
	memcpy(mapped_res.pData,addr,size);
	pContext->Unmap(m_pD3D11Buffer, 0);
	if(m_pD3DX11EffectConstantBuffer)
		m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
}

void dx11::PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &deviceContext)
{
	if(m_pD3DX11EffectConstantBuffer)
		m_pD3DX11EffectConstantBuffer->SetConstantBuffer(NULL);
}

dx11::Effect::Effect(void *device,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	ID3DX11Effect *e=NULL;
	HRESULT hr=CreateEffect((ID3D11Device *)device,&e,filename_utf8,defines,D3DCOMPILE_OPTIMIZATION_LEVEL3);
	platform_effect=e;
}

dx11::Effect::~Effect()
{
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	SAFE_RELEASE(e);
	for(TechniqueMap::iterator i=techniques.begin();i!=techniques.end();i++)
	{
		delete i->second;
	}
}

crossplatform::EffectTechnique *dx11::Effect::GetTechniqueByName(const char *name)
{
	if(techniques.find(name)!=techniques.end())
	{
		return techniques[name];
	}
	if(!platform_effect)
		return NULL;
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	crossplatform::EffectTechnique *tech=new crossplatform::EffectTechnique;
	tech->platform_technique=e->GetTechniqueByName(name);
	techniques[name]=tech;
	return tech;
}

void dx11::Effect::SetTexture(const char *name,void *tex)
{
	simul::dx11::setTexture(asD3DX11Effect(),name,(ID3D11ShaderResourceView *)tex);
}

void dx11::Effect::SetTexture(const char *name,crossplatform::Texture &t)
{
	dx11::Texture *T=(dx11::Texture*)&t;
	simul::dx11::setTexture(asD3DX11Effect(),name,T->shaderResourceView);
}

