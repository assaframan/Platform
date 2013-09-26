#define NOMINMAX
#include "Simul/Platform/DirectX9/Framebuffer.h"
#include "Simul/Platform/DirectX9/Macros.h"
#include "Simul/Platform/DirectX9/CreateDX9Effect.h"

Framebuffer::Framebuffer()
	:m_pd3dDevice(NULL)
	,buffer_depth_texture(NULL)
	,buffer_texture(NULL)
	,m_pHDRRenderTarget(NULL)
	,m_pBufferDepthSurface(NULL)
	,m_pOldRenderTarget(NULL)
	,m_pOldDepthSurface(NULL)
	,depth_format((D3DFORMAT)0)
{
	texture_format=D3DFMT_A16B16G16R16F;
	texture_format=D3DFMT_A32B32G32R32F;
}

Framebuffer::~Framebuffer()
{
}

void Framebuffer::SetWidthAndHeight(int w,int h)
{
	if(buffer_texture!=NULL&&w==Width&&h==Height)
		return;
	Width=w;
	Height=h;
	MakeTexture();
}
void Framebuffer::MakeTexture()
{
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(buffer_texture);
	SAFE_RELEASE(m_pHDRRenderTarget);
	if(!Width||!Height)
		return;
	m_pd3dDevice->CreateTexture(	Width,
									Height,
									1,
									D3DUSAGE_RENDERTARGET,
									texture_format,
									D3DPOOL_DEFAULT,
									&buffer_texture,
									NULL);
	m_pHDRRenderTarget=MakeRenderTarget(buffer_texture);
}

void Framebuffer::SetFormat(int f)
{
	D3DFORMAT F=(D3DFORMAT)f;
	bool ok=CanUseTexFormat(m_pd3dDevice,F)==S_OK;
	if(ok)
	{
		if(texture_format==F)
			return;
		texture_format=F;
		MakeTexture();
	}
}
	
void Framebuffer::SetDepthFormat(int f)
{
	D3DFORMAT F=(D3DFORMAT)f;
	bool ok=CanUseTexFormat(m_pd3dDevice,F)==S_OK;
	if(ok)
	{
		if(depth_format==F)
			return;
		depth_format=F;
	}
}
	
void Framebuffer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	MakeTexture();
}

void Framebuffer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(buffer_texture);
	SAFE_RELEASE(buffer_depth_texture);
	SAFE_RELEASE(m_pHDRRenderTarget);
	SAFE_RELEASE(m_pBufferDepthSurface);
}

void Framebuffer::Activate(void *)
{
	m_pOldRenderTarget=NULL;
	m_pOldDepthSurface=NULL;
	D3DSURFACE_DESC desc;
	V_CHECK(buffer_texture->GetLevelDesc(0,&desc));
	V_CHECK(m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget));
	m_pOldRenderTarget->GetDesc(&desc);
	V_CHECK(m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface));
	V_CHECK(m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget));
	if(m_pBufferDepthSurface)
		V_CHECK(m_pd3dDevice->SetDepthStencilSurface(m_pBufferDepthSurface));
	D3DVIEWPORT9 viewport;
	viewport.Width	=Width;
	viewport.Height	=Height;
	viewport.X		=0;
	viewport.Y		=0;
	viewport.MinZ	=0.f;
	viewport.MaxZ	=1.f;
	V_CHECK(m_pd3dDevice->SetViewport(&viewport));
}

void Framebuffer::ActivateViewport(void* context,float viewportX, float viewportY, float viewportW, float viewportH)
{
	Activate(context);
	D3DVIEWPORT9 viewport;
	viewport.Width	=(int)(viewportW*Width);
	viewport.Height	=(int)(viewportH*Height);
	viewport.X		=(int)(viewportX*Width);
	viewport.Y		=(int)(viewportY*Height);
	viewport.MinZ	=0.f;
	viewport.MaxZ	=1.f;
	m_pd3dDevice->SetViewport(&viewport);
}

void Framebuffer::Deactivate(void *)
{
	//m_pOldRenderTarget->GetDesc(&desc);
	m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
	if(m_pOldDepthSurface)
		m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
}
void Framebuffer::Clear(void *,float r,float g,float b,float a,float depth,int mask)
{
	// Don't yet support reverse depth on dx9.
	if(!mask)
		mask=D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER;
	m_pd3dDevice->Clear(0L,NULL,mask,D3DCOLOR_COLORVALUE(r,g,b,a),depth,0L);
}

void Framebuffer::ClearColour(void *context,float r,float g,float b,float a)
{
	Clear(context,r,g,b,a,0.f,D3DCLEAR_TARGET);
}