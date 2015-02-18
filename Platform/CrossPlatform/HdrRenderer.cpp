#include "HdrRenderer.h"
#include <string>
#include <algorithm>			// for std::min / max
#include <assert.h>
#include <stdarg.h>
#include "Simul/Base/Timer.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Math/Pi.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
using namespace simul;
using namespace crossplatform;

HdrRenderer::HdrRenderer()
	:renderPlatform(NULL)
	,hdr_effect(NULL)
	,m_pGaussianEffect(NULL)
	,Width(0)
	,Height(0)
	,exposureGammaTechnique(NULL)
	,glowExposureGammaTechnique(NULL)
	,glowTechnique(NULL)
	,Glow(false)
	,ReverseDepth(false)
{
	for(int i=0;i<4;i++)
	{
		brightpassTextures[i]=NULL;
		glowTextures[i]=NULL;
	}
}

HdrRenderer::~HdrRenderer()
{
	InvalidateDeviceObjects();
}

void HdrRenderer::SetBufferSize(int w,int h)
{
	if(Width==w&&Height==h)
		return;
	if(!renderPlatform)
		return;
	Width=w;
	Height=h;
	if(Width>0&&Height>0)
	{
		int H=Height;
		int W=Width;
		for(int i=0;i<4;i++)
		{
			W/=2;
			H/=2;
			brightpassTextures[i]->ensureTexture2DSizeAndFormat(renderPlatform,W,H,crossplatform::RGBA_16_FLOAT,false,true);
			glowTextures[i]->ensureTexture2DSizeAndFormat(renderPlatform,W,H,crossplatform::R_32_UINT,true,false);
		}
	}
	RecompileShaders();
}

void HdrRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	for(int i=0;i<4;i++)
	{
		SAFE_DELETE(brightpassTextures[i]);
		brightpassTextures[i]=renderPlatform->CreateTexture();
		SAFE_DELETE(glowTextures[i]);
		glowTextures[i]=renderPlatform->CreateTexture();
	}
	hdrConstants.RestoreDeviceObjects(renderPlatform);
	imageConstants.RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
}

static std::string string_format(const std::string fmt, ...)
{
    int size = 100;
    std::string str;
    va_list ap;
    while (1)
	{
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.c_str(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size)
		{
            str.resize(n);
            return str;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }
    return str;
}

template<typename t> t max3(t a,t b,t c)
{
	return std::max(a,std::max(b,c));
}
	static int	threadsPerGroup = 128;

void HdrRenderer::RecompileShaders()
{
	SAFE_DELETE(hdr_effect);
	SAFE_DELETE(m_pGaussianEffect);
	if(!renderPlatform)
		return;
	std::map<std::string,std::string> defs;
	hdr_effect					=renderPlatform->CreateEffect("hdr",defs);
	exposureGammaTechnique		=hdr_effect->GetTechniqueByName("exposure_gamma");
	glowExposureGammaTechnique	=hdr_effect->GetTechniqueByName("glow_exposure_gamma");
	warpExposureGamma			=hdr_effect->GetTechniqueByName("warp_exposure_gamma");
	warpGlowExposureGamma		=hdr_effect->GetTechniqueByName("warp_glow_exposure_gamma");
	glowTechnique				=hdr_effect->GetTechniqueByName("simul_glow");
	hdrConstants.LinkToEffect(hdr_effect,"HdrConstants");
	
	// Just set scan_mem_size to the largest value we can ever expect:
	int scan_smem_size			=1920;//max3(H,W,(int)threadsPerGroup*2);//1920;//
	defs["SCAN_SMEM_SIZE"]		=string_format("%d",scan_smem_size);
	defs["THREADS_PER_GROUP"]	=string_format("%d",threadsPerGroup);
	
	m_pGaussianEffect			=renderPlatform->CreateEffect("gaussian",defs);
	hdrConstants.LinkToEffect(m_pGaussianEffect,"HdrConstants");
	imageConstants.LinkToEffect(m_pGaussianEffect,"ImageConstants");
}

void HdrRenderer::InvalidateDeviceObjects()
{
	hdrConstants.InvalidateDeviceObjects();
	imageConstants.InvalidateDeviceObjects();
	for(int i=0;i<4;i++)
	{
		SAFE_DELETE(brightpassTextures[i]);
		SAFE_DELETE(glowTextures[i]);
	}
	SAFE_DELETE(hdr_effect);
	SAFE_DELETE(m_pGaussianEffect);
	renderPlatform=NULL;
}
void HdrRenderer::Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma)
{
	Render(deviceContext,texture,0,Exposure, Gamma);
}

void HdrRenderer::Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,float offsetX,float Exposure,float Gamma)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"HDR")
	hdrConstants.gamma		=Gamma;
	hdrConstants.exposure	=Exposure;
	hdrConstants.offset		=vec2(offsetX,0.0f);
	hdrConstants.Apply(deviceContext);
	crossplatform::EffectTechnique *tech=exposureGammaTechnique;
	if(Glow)
	{
		RenderGlowTexture(deviceContext,texture);
		tech=glowExposureGammaTechnique;
		hdr_effect->SetTexture(deviceContext,"glowTexture",glowTextures[0]);
	}
	bool msaa=(texture->GetSampleCount()>1);
	if(msaa)
		hdr_effect->SetTexture(deviceContext,"imageTextureMS"	,texture);
	else
		hdr_effect->SetTexture(deviceContext,"imageTexture"	,texture);
	hdr_effect->Apply(deviceContext,tech,(msaa?"msaa":"main"));
	renderPlatform->DrawQuad(deviceContext);

	hdr_effect->SetTexture(deviceContext,"imageTexture",NULL);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",NULL);
	hdrConstants.Unbind(deviceContext);
	imageConstants.Unbind(deviceContext);
	
	hdr_effect->Unapply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}
void HdrRenderer::RenderInfraRed(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,vec3 infrared_integration_factors,float Exposure,float Gamma)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"RenderInfraRed")
	bool msaa=(texture->GetSampleCount()>1);
	if(msaa)
		hdr_effect->SetTexture(deviceContext,"imageTextureMS"	,texture);
	else
		hdr_effect->SetTexture(deviceContext,"imageTexture"	,texture);
	hdrConstants.gamma						=Gamma;
	hdrConstants.exposure					=Exposure;
	hdrConstants.infraredIntegrationFactors	=infrared_integration_factors;
	hdrConstants.offset						=vec2(0.f,0.f);
	hdrConstants.Apply(deviceContext);
	crossplatform::EffectTechnique *tech=hdr_effect->GetTechniqueByName("infra_red");
	hdr_effect->Apply(deviceContext,tech,(msaa?"msaa":"main"));
	renderPlatform->DrawQuad(deviceContext);

	hdr_effect->SetTexture(deviceContext,"imageTexture",NULL);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",NULL);
	hdrConstants.Unbind(deviceContext);
	imageConstants.Unbind(deviceContext);
	hdr_effect->Unapply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}


void HdrRenderer::RenderWithOculusCorrection(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture
	,float offsetX,float Exposure,float Gamma)
{
hdr_effect->SetTexture(deviceContext,"imageTexture",texture);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",texture);
	hdrConstants.gamma				=Gamma;
	hdrConstants.exposure			=Exposure;
	
	float direction=(offsetX-0.5f)*2.0f;


    float as = float(640) / float(800);
	
	vec4 distortionK(1.0f,0.22f,0.24f,0.0f);
    // We are using 1/4 of DistortionCenter offset value here, since it is
    // relative to [-1,1] range that gets mapped to [0, 0.5].
	static float xco				= 0.15197642f;
	float Distortion_XCenterOffset	= direction*xco;
	static float Distortion_Scale	= 1.7146056f;
    float scaleFactor				=1.0f/Distortion_Scale;
	hdrConstants.warpHmdWarpParam	=distortionK;
	hdrConstants.warpLensCentre		=vec2(0.5f+Distortion_XCenterOffset*0.5f, 0.5f);
	hdrConstants.warpScreenCentre	=vec2(0.5f,0.5f);
	hdrConstants.warpScale			=vec2(0.5f* scaleFactor, 0.5f* scaleFactor * as);
	hdrConstants.warpScaleIn		=vec2(2.f,2.f/ as);
	hdrConstants.offset				=vec2(offsetX,0.0f);
	hdrConstants.Apply(deviceContext);
	if(Glow)
	{
		RenderGlowTexture(deviceContext,texture);
		hdr_effect->SetTexture(deviceContext,"glowTexture",glowTextures[0]);
		hdr_effect->Apply(deviceContext,warpGlowExposureGamma,0);
	}
	else
		hdr_effect->Apply(deviceContext,warpExposureGamma,0);
	renderPlatform->DrawQuad(deviceContext);
	
	hdr_effect->SetTexture(deviceContext,"imageTexture",NULL);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",NULL);
	hdrConstants.Unbind(deviceContext);
	hdr_effect->Unapply(deviceContext);
}

static float CalculateBoxFilterWidth(float radius, int pass)
{
	// Calculate standard deviation according to cutoff width
	// We use sigma*3 as the width of filtering window
	float sigma = radius / 3.0f;
	// The width of the repeating box filter
	float box_width = sqrt(sigma * sigma * 12.0f / pass + 1);
	return box_width;
}

void HdrRenderer::RenderGlowTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture)
{
	if(!m_pGaussianEffect)
		return;
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"RenderGlowTexture")
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"downscale")
		
	// Render to the low-res glow.
	if(glowTechnique)
	{
		hdr_effect->SetTexture(deviceContext,"imageTexture",texture);
		hdr_effect->SetTexture(deviceContext,"imageTextureMS",texture);
		hdrConstants.offset				=vec2(1.f/Width,1.f/Height);
		hdrConstants.Apply(deviceContext);
		hdr_effect->Apply(deviceContext,glowTechnique,(0));
		brightpassTextures[0]->activateRenderTarget(deviceContext);
		//glow_fb->Clear(deviceContext, 0, 0, 0, 0, 0);
		renderPlatform->DrawQuad(deviceContext);
		brightpassTextures[0]->deactivateRenderTarget();
		hdr_effect->Unapply(deviceContext);
		
		for(int i=1;i<4;i++)
		{
			hdr_effect->SetTexture(deviceContext,"imageTexture",brightpassTextures[i-1]);
			brightpassTextures[i]->activateRenderTarget(deviceContext);
			hdr_effect->Apply(deviceContext,hdr_effect->GetTechniqueByName("downscale2"),0);
			renderPlatform->DrawQuad(deviceContext);
			hdr_effect->Unapply(deviceContext);
			brightpassTextures[i]->deactivateRenderTarget();
		}
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	for(int i=0;i<4;i++)
	{
		char c[]={'0',0};
		c[0]='0'+i;
		SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,c)
		DoGaussian(deviceContext,brightpassTextures[i],glowTextures[i]);
		SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	}

	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}
void HdrRenderer::DoGaussian(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *brightpassTexture,crossplatform::Texture *targetTexture)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"H")
	static int g_NumApproxPasses	=3;
	static int	g_MaxApproxPasses	=8;
	static float g_FilterRadius		=30;
	float box_width					= CalculateBoxFilterWidth(g_FilterRadius, g_NumApproxPasses);
	float half_box_width			= box_width * 0.5f;
	float frac_half_box_width		= (half_box_width + 0.5f) - (int)(half_box_width + 0.5f);
	float inv_frac_half_box_width	= 1.0f - frac_half_box_width;
	float rcp_box_width				= 1.0f / box_width;
	// Step 1. Vertical passes: Each thread group handles a column in the image
	// Input texture
	m_pGaussianEffect->SetTexture(deviceContext,"g_texInput",brightpassTextures[0]);
	// Output texture
	m_pGaussianEffect->SetUnorderedAccessView(deviceContext,"g_rwtOutput",glowTextures[0]);
	imageConstants.imageSize					=uint2(brightpassTextures[0]->width,brightpassTextures[0]->length);
	// Each thread is a chunk of threadsPerGroup(=128) texels, so to cover all of them we divide by threadsPerGroup
	imageConstants.texelsPerThread				=(brightpassTextures[0]->length + threadsPerGroup - 1)/threadsPerGroup;
	imageConstants.g_NumApproxPasses			=g_NumApproxPasses-1;
	imageConstants.g_HalfBoxFilterWidth			=half_box_width;
	imageConstants.g_FracHalfBoxFilterWidth		=frac_half_box_width;
	imageConstants.g_InvFracHalfBoxFilterWidth	=inv_frac_half_box_width;
	imageConstants.g_RcpBoxFilterWidth			=rcp_box_width;
	imageConstants.Apply(deviceContext);
	// Select pass
	gaussianColTechnique						=m_pGaussianEffect->GetTechniqueByName("simul_gaussian_col");
	m_pGaussianEffect->Apply(deviceContext,gaussianColTechnique,0);
	// We perform the Gaussian blur for each column. Each group is a column, and each thread 
	renderPlatform->DispatchCompute(deviceContext,brightpassTextures[0]->width,1,1);
	m_pGaussianEffect->UnbindTextures(deviceContext);
	
	m_pGaussianEffect->Unapply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)

	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"W")
	// Step 2. Horizontal passes: Each thread group handles a row in the image
	// Input texture
	m_pGaussianEffect->SetTexture(deviceContext,"g_texInput",brightpassTextures[0]);
	// Output texture
	m_pGaussianEffect->SetUnorderedAccessView(deviceContext,"g_rwtOutput",glowTextures[0]);
	imageConstants.texelsPerThread				=(brightpassTextures[0]->width + threadsPerGroup - 1)/threadsPerGroup;
	imageConstants.Apply(deviceContext);
	// Select pass
	gaussianRowTechnique = m_pGaussianEffect->GetTechniqueByName("simul_gaussian_row");
	m_pGaussianEffect->Apply(deviceContext,gaussianRowTechnique,0);
	renderPlatform->DispatchCompute(deviceContext,brightpassTextures[0]->length,1,1);
	m_pGaussianEffect->UnbindTextures(deviceContext);
	m_pGaussianEffect->SetUnorderedAccessView(deviceContext,"g_rwtOutput",NULL);
	m_pGaussianEffect->Unapply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}

void HdrRenderer::RenderDebug(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	int w=width/2-8;
	int h=(int)(w*((float)brightpassTextures[0]->length/(float)brightpassTextures[0]->width));
	int x=x0;
	int y=y0;
	int y1=y+h+8;
	for(int i=0;i<4;i++)
	{
		renderPlatform->DrawTexture(deviceContext,x,y,w,h,brightpassTextures[i]);
		renderPlatform->DrawTexture(deviceContext,x,y1,w,h,glowTextures[i]);
		x+=w;
		w/=2;
		h/=2;
	}
}