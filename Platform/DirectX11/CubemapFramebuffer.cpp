#define NOMINMAX
#include "CubemapFramebuffer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "D3dx11effect.h"
#include <assert.h>
const int MIPLEVELS=1;

using namespace simul;
using namespace dx11;

CubemapFramebuffer::CubemapFramebuffer()
	:bands(4)
	,texture(NULL)
	,depth_texture(NULL)
	,Width(0)
	,Height(0)
	,current_face(-1)
	,format(crossplatform::RGBA_8_UNORM)
	,depth_format(crossplatform::UNKNOWN)
	,sphericalHarmonicsEffect(NULL)
{
	for(int i=0;i<6;i++)
	{
		m_pCubeEnvMapRTV[i]=NULL;
		//m_pCubeEnvDepthMap[i]	=NULL;
	}
}

CubemapFramebuffer::~CubemapFramebuffer()
{
	InvalidateDeviceObjects();
}

void CubemapFramebuffer::SetWidthAndHeight(int w,int h)
{
	Width=w;
	Height=h;
	assert(h==w);
}

void CubemapFramebuffer::SetFormat(crossplatform::PixelFormat f)
{
	if(f==format)
		return;
	format=f;
}

void CubemapFramebuffer::SetDepthFormat(crossplatform::PixelFormat f)
{
	if(f==depth_format)
		return;
	depth_format=f;
}

void CubemapFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform	*r)
{
	renderPlatform=r;
	CreateBuffers();
}

bool CubemapFramebuffer::IsValid() const
{
	return (texture!=NULL);
}

bool CubemapFramebuffer::CreateBuffers()
{
	for(int i=0;i<6;i++)
	{
		SAFE_RELEASE(m_pCubeEnvMapRTV[i]);
//		SAFE_DELETE(m_pCubeEnvDepthMap[i]);
	//	m_pCubeEnvDepthMap[i]=renderPlatform->CreateTexture();
	}
	SAFE_DELETE(texture);
	SAFE_DELETE(depth_texture);
	texture=renderPlatform->CreateTexture();
	depth_texture=renderPlatform->CreateTexture();
	// The table of coefficients.
	int s=(bands+1);
	if(s<4)
		s=4;
	sphericalHarmonics.InvalidateDeviceObjects();
	sphericalSamples.InvalidateDeviceObjects();
	// Create cubic depth stencil texture
 
	sphericalHarmonicsConstants.RestoreDeviceObjects(renderPlatform);
	// Create the depth stencil view for the entire cube
	D3D11_DEPTH_STENCIL_VIEW_DESC DescDS;
    ZeroMemory( &DescDS, sizeof( DescDS ) );
	DescDS.Format = DXGI_FORMAT_D32_FLOAT;
	DescDS.ViewDimension				=D3D11_DSV_DIMENSION_TEXTURE2D;
	DescDS.Texture2D.MipSlice			=0;
 
	D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc;
	ZeroMemory(&depthSRVDesc,sizeof(depthSRVDesc));
	depthSRVDesc.Format						=DXGI_FORMAT_R32_FLOAT;
	depthSRVDesc.ViewDimension				=D3D11_SRV_DIMENSION_TEXTURE2D;
	depthSRVDesc.Texture2D.MipLevels		=MIPLEVELS;
	depthSRVDesc.Texture2D.MostDetailedMip	=0;

	if(depth_format!=crossplatform::UNKNOWN)
		depth_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,depth_format,false,false,true);

	// Create the cube map for env map render target
	D3D11_TEXTURE2D_DESC tex2dDesc;
	tex2dDesc.Width					=Width;
	tex2dDesc.Height				=Width;
	tex2dDesc.ArraySize				=6;
	tex2dDesc.SampleDesc.Count		=1;
	tex2dDesc.SampleDesc.Quality	=0;
	tex2dDesc.Usage					=D3D11_USAGE_DEFAULT;
	tex2dDesc.CPUAccessFlags		=0;
	tex2dDesc.Format				=RenderPlatform::ToDxgiFormat(format);
	tex2dDesc.BindFlags				=D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	tex2dDesc.MiscFlags				=D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;
	tex2dDesc.MipLevels				=MIPLEVELS;
	dx11::Texture *t				=(dx11::Texture *)texture;
	ID3D11Texture2D *tex2d			=(ID3D11Texture2D*)t->texture;
	V_CHECK(renderPlatform->AsD3D11Device()->CreateTexture2D(&tex2dDesc,NULL,&tex2d));
	t->texture=tex2d;

	// Create the 6-face render target view
	D3D11_RENDER_TARGET_VIEW_DESC DescRT;
	DescRT.Format							=tex2dDesc.Format;
	DescRT.ViewDimension					=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	DescRT.Texture2DArray.FirstArraySlice	=0;
	DescRT.Texture2DArray.ArraySize			=6;
	DescRT.Texture2DArray.MipSlice			=0;
	 
	for(int i=0;i<6;i++)
	{
		DescRT.Texture2DArray.FirstArraySlice = i;
		DescRT.Texture2DArray.ArraySize = 1;
		V_CHECK(renderPlatform->AsD3D11Device()->CreateRenderTargetView(tex2d, &DescRT, &(m_pCubeEnvMapRTV[i])));
	}
	// Create the shader resource view for the cubic env map
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format						=tex2dDesc.Format;
	SRVDesc.ViewDimension				=D3D11_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels		=MIPLEVELS;
	SRVDesc.TextureCube.MostDetailedMip =0;
	 
	V_CHECK( renderPlatform->AsD3D11Device()->CreateShaderResourceView(tex2d, &SRVDesc, &t->shaderResourceView ));
	
	return true;
}

ID3D11Texture2D* makeStagingTexture(ID3D11Device *pd3dDevice,int w,DXGI_FORMAT target_format)
{
	D3D11_TEXTURE2D_DESC tex2dDesc;
	tex2dDesc.Width					= w;
	tex2dDesc.Height				= w;
	tex2dDesc.MipLevels				= 1;
	tex2dDesc.ArraySize				= 6;
	tex2dDesc.SampleDesc.Count		= 1;
	tex2dDesc.SampleDesc.Quality	= 0;
	tex2dDesc.Format				= target_format;
	tex2dDesc.Usage					= D3D11_USAGE_STAGING;
	tex2dDesc.BindFlags				= 0;
	tex2dDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_READ| D3D11_CPU_ACCESS_WRITE;
	tex2dDesc.MiscFlags				=D3D11_RESOURCE_MISC_TEXTURECUBE;
	ID3D11Texture2D* tex			=NULL;
	pd3dDevice->CreateTexture2D(&tex2dDesc,NULL,&tex);
	return tex;
}

void CubemapFramebuffer::InvalidateDeviceObjects()
{
	sphericalHarmonicsConstants.InvalidateDeviceObjects();
	SAFE_DELETE(texture);
	SAFE_DELETE(depth_texture);
	for(int i=0;i<6;i++)
	{
//		SAFE_DELETE(m_pCubeEnvDepthMap[i]);
		SAFE_RELEASE(m_pCubeEnvMapRTV[i]);
	}
	sphericalHarmonics.InvalidateDeviceObjects();
	SAFE_DELETE(sphericalHarmonicsEffect);
	sphericalSamples.InvalidateDeviceObjects();
}

void CubemapFramebuffer::SetCurrentFace(int i)
{
	current_face=i;
}

ID3D11Texture2D* CubemapFramebuffer::GetCopy(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	D3D11_BOX sourceRegion;
	sourceRegion.left = 0;
	sourceRegion.right = Width;
	sourceRegion.top = 0;
	sourceRegion.bottom = Height;
	sourceRegion.front = 0;
	sourceRegion.back = 1;
	 dx11::Texture *t=(dx11::Texture *)texture;
	 ID3D11Texture2D *tex2d=(ID3D11Texture2D*)t->texture;
	 ID3D11Texture2D* stagingTexture=makeStagingTexture(renderPlatform->AsD3D11Device(),Width,RenderPlatform::ToDxgiFormat(format));
	for(int i=0;i<6;i++)
		pContext->CopySubresourceRegion(stagingTexture,i, 0, 0, 0, tex2d,i, &sourceRegion);
	return stagingTexture;
}

void CubemapFramebuffer::RecompileShaders()
{
	SAFE_DELETE(sphericalHarmonicsEffect);
	if(!renderPlatform)
		return;
	sphericalHarmonicsEffect=renderPlatform->CreateEffect("spherical_harmonics");
	sphericalHarmonicsConstants.LinkToEffect(sphericalHarmonicsEffect,"SphericalHarmonicsConstants");
}

void CubemapFramebuffer::CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!sphericalHarmonicsEffect)
		RecompileShaders();
	int num_coefficients=bands*bands;
	static int BLOCK_SIZE=4;
	static int sqrt_jitter_samples					=4;
	if(!sphericalHarmonics.count)
	{
		sphericalHarmonics.RestoreDeviceObjects(renderPlatform,num_coefficients,true);
		sphericalSamples.RestoreDeviceObjects(renderPlatform,sqrt_jitter_samples*sqrt_jitter_samples,true);
	}
	sphericalHarmonicsConstants.num_bands			=bands;
	sphericalHarmonicsConstants.sqrtJitterSamples	=sqrt_jitter_samples;
	sphericalHarmonicsConstants.numJitterSamples	=sqrt_jitter_samples*sqrt_jitter_samples;
	sphericalHarmonicsConstants.invNumJitterSamples	=1.0f/(float)sphericalHarmonicsConstants.numJitterSamples;
	sphericalHarmonicsConstants.Apply(deviceContext);
	simul::dx11::setUnorderedAccessView	(sphericalHarmonicsEffect->asD3DX11Effect(),"targetBuffer"	,sphericalHarmonics.AsD3D11UnorderedAccessView());
	ID3DX11EffectTechnique *clear		=sphericalHarmonicsEffect->asD3DX11Effect()->GetTechniqueByName("clear");
	ApplyPass(pContext,clear->GetPassByIndex(0));
	pContext->Dispatch((num_coefficients+BLOCK_SIZE-1)/BLOCK_SIZE,1,1);
	{
		// The table of 3D directional sample positions. sqrt_jitter_samples x sqrt_jitter_samples
		// We just fill this texture with random 3d directions.
		ID3DX11EffectTechnique *jitter=sphericalHarmonicsEffect->asD3DX11Effect()->GetTechniqueByName("jitter");
		simul::dx11::setUnorderedAccessView(sphericalHarmonicsEffect->asD3DX11Effect(),"samplesBufferRW",sphericalSamples.AsD3D11UnorderedAccessView());
		ApplyPass(pContext,jitter->GetPassByIndex(0));
		pContext->Dispatch((sqrt_jitter_samples+BLOCK_SIZE-1)/BLOCK_SIZE,(sqrt_jitter_samples+BLOCK_SIZE-1)/BLOCK_SIZE,1);
		simul::dx11::setUnorderedAccessView(sphericalHarmonicsEffect->asD3DX11Effect(),"samplesBufferRW",NULL);
		ApplyPass(pContext,jitter->GetPassByIndex(0));
	}

	ID3DX11EffectTechnique *tech		=sphericalHarmonicsEffect->asD3DX11Effect()->GetTechniqueByName("encode");
	simul::dx11::setTexture				(sphericalHarmonicsEffect->asD3DX11Effect(),"cubemapTexture"	,texture->AsD3D11ShaderResourceView());
	simul::dx11::setTexture				(sphericalHarmonicsEffect->asD3DX11Effect(),"samplesBuffer"		,sphericalSamples.AsD3D11ShaderResourceView());
	
	static bool sh_by_samples=false;
	ApplyPass(pContext,tech->GetPassByIndex(0));
	pContext->Dispatch(((sh_by_samples?sphericalHarmonicsConstants.numJitterSamples:num_coefficients)+BLOCK_SIZE-1)/BLOCK_SIZE,1,1);
	simul::dx11::setTexture				(sphericalHarmonicsEffect->asD3DX11Effect(),"cubemapTexture"	,NULL);
	simul::dx11::setUnorderedAccessView	(sphericalHarmonicsEffect->asD3DX11Effect(),"targetBuffer"	,NULL);
	simul::dx11::setTexture				(sphericalHarmonicsEffect->asD3DX11Effect(),"samplesBuffer"	,NULL);
	sphericalHarmonicsConstants.Unbind(deviceContext);
	ApplyPass(pContext,tech->GetPassByIndex(0));
}

void CubemapFramebuffer::Activate(crossplatform::DeviceContext &deviceContext)
{
	ActivateViewport(deviceContext,0.f,0.f,1.f,1.f);
}

void CubemapFramebuffer::ActivateDepth(crossplatform::DeviceContext &)
{
}

void CubemapFramebuffer::ActivateColour(crossplatform::DeviceContext &context,const float viewportXYWH[4])
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context.asD3D11DeviceContext();
	
	unsigned int num_v=0;
	pContext->RSGetViewports(&num_v,NULL);
	if(num_v<=4)
		pContext->RSGetViewports(&num_v,m_OldViewports);

	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	pContext->OMGetRenderTargets(	1,
									&m_pOldRenderTarget,
									&m_pOldDepthSurface
									);
	pContext->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],NULL);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width		=floorf((float)Width*viewportXYWH[2] + 0.5f);
	viewport.Height		=floorf((float)Height*viewportXYWH[3] + 0.5f);
	viewport.MinDepth	=0.0f;
	viewport.MaxDepth	=1.0f;
	viewport.TopLeftX	=floorf((float)Width*viewportXYWH[0] + 0.5f);
	viewport.TopLeftY	=floorf((float)Height*viewportXYWH[1]+ 0.5f);

	// Create the viewport.
	pContext->RSSetViewports(1, &viewport);
}

void CubemapFramebuffer::ActivateViewport(crossplatform::DeviceContext &deviceContext, float viewportX, float viewportY, float viewportW, float viewportH)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	
	unsigned int num_v=0;
	pContext->RSGetViewports(&num_v,NULL);
	if(num_v<=4)
		pContext->RSGetViewports(&num_v,m_OldViewports);

	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	pContext->OMGetRenderTargets(	1,
									&m_pOldRenderTarget,
									&m_pOldDepthSurface
									);
	pContext->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],depth_texture?depth_texture->AsD3D11DepthStencilView():NULL);//m_pCubeEnvDepthMap[current_face]->AsD3D11DepthStencilView());
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width = floorf((float)Width*viewportW + 0.5f);
	viewport.Height = floorf((float)Height*viewportH + 0.5f);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = floorf((float)Width*viewportX + 0.5f);
	viewport.TopLeftY = floorf((float)Height*viewportY + 0.5f);

	// Create the viewport.
	pContext->RSSetViewports(1, &viewport);
}

void CubemapFramebuffer::Deactivate(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	// Create the viewport.
	pContext->RSSetViewports(1,m_OldViewports);
}

void CubemapFramebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],NULL);
}

void CubemapFramebuffer::Clear(crossplatform::DeviceContext &deviceContext, float r, float g, float b, float a, float depth, int mask)
{
	ID3D11DeviceContext *pContext = deviceContext.asD3D11DeviceContext();
	if(!mask)
		mask=D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL;
	// Clear the screen to the colour specified:
    float clearColor[4]={r,g,b,a};
	if(current_face>=0)
	{
		pContext->ClearRenderTargetView(m_pCubeEnvMapRTV[current_face],clearColor);
	}
	else
	{
		for(int i=0;i<6;i++)
		{
			pContext->ClearRenderTargetView(m_pCubeEnvMapRTV[i],clearColor);
		//	if(m_pCubeEnvDepthMap[i]->AsD3D11DepthStencilView())
		//		pContext->ClearDepthStencilView(m_pCubeEnvDepthMap[i]->AsD3D11DepthStencilView(),mask,depth, 0);
		}
	}
		if(depth_texture&&depth_texture->AsD3D11DepthStencilView())
			pContext->ClearDepthStencilView(depth_texture->AsD3D11DepthStencilView(),mask,depth,0);
}

void CubemapFramebuffer::ClearColour(crossplatform::DeviceContext &deviceContext, float r, float g, float b, float a)
{
	float clearColor[4]={r,g,b,a};
	for(int i=0;i<6;i++)
	{
		deviceContext.asD3D11DeviceContext()->ClearRenderTargetView(m_pCubeEnvMapRTV[i], clearColor);
	}
}