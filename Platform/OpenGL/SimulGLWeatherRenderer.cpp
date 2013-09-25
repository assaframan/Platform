
#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#endif

#include "FramebufferGL.h"
#include "LoadGLProgram.h"
#include "SimulGLWeatherRenderer.h"
#include "SimulGLSkyRenderer.h"
#include "SimulGLCloudRenderer.h"
#include "SimulGL2DCloudRenderer.h"
#include "SimulGLLightningRenderer.h"
#include "SimulGLAtmosphericsRenderer.h"
#include "SimulGLPrecipitationRenderer.h"
#include "SimulGLUtilities.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/Decay.h"

using namespace simul;
using namespace opengl;

static const GLenum buffer_tex_format		=GL_FLOAT;
static const GLenum internal_buffer_format	=GL_RGBA32F_ARB;

SimulGLWeatherRenderer::SimulGLWeatherRenderer(simul::clouds::Environment *env
											   ,simul::base::MemoryInterface *mem
											   ,int width
											   ,int height
											   ,bool sky
											   ,bool rain)
		:BaseWeatherRenderer(env,mem)
		,BufferWidth(0)
		,BufferHeight(0)
		,device_initialized(false)
		,scene_buffer(NULL)
		,cloud_overlay_program(0)
{
	simul::sky::SkyKeyframer *sk=environment->skyKeyframer;
	simul::clouds::CloudKeyframer *ck2d=environment->cloud2DKeyframer;
	simul::clouds::CloudKeyframer *ck3d=environment->cloudKeyframer;
	simulSkyRenderer					=::new(memoryInterface) SimulGLSkyRenderer(sk,memoryInterface);
	baseSkyRenderer=simulSkyRenderer;
	
	simulCloudRenderer					=::new(memoryInterface) SimulGLCloudRenderer(ck3d,mem);
	baseCloudRenderer=simulCloudRenderer;
	if(env->cloud2DKeyframer)
	{
		base2DCloudRenderer				=simul2DCloudRenderer
										=::new SimulGL2DCloudRenderer(ck2d,mem);
	}	
	simulLightningRenderer				=::new(memoryInterface) SimulGLLightningRenderer(ck3d,sk);
	baseLightningRenderer=simulLightningRenderer;

	simulAtmosphericsRenderer			=::new(memoryInterface) SimulGLAtmosphericsRenderer(mem);
	baseAtmosphericsRenderer=simulAtmosphericsRenderer;
	if(rain)
		basePrecipitationRenderer		=simulPrecipitationRenderer
										=::new(memoryInterface) SimulGLPrecipitationRenderer();

	EnableCloudLayers();
	SetScreenSize(width,height);
}


void SimulGLWeatherRenderer::EnableCloudLayers()
{
	if(simulCloudRenderer)
		simulCloudRenderer->Create();
	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->Create();
		if(simulSkyRenderer)
			simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetBaseSkyInterface());
	}
	if(device_initialized)
	{
		if(simulSkyRenderer)
			simulSkyRenderer->RestoreDeviceObjects(NULL);
		if(simulPrecipitationRenderer)
			simulPrecipitationRenderer->RestoreDeviceObjects(NULL);
		if(simul2DCloudRenderer)
			simul2DCloudRenderer->RestoreDeviceObjects(NULL);
		if(simulCloudRenderer)
			simulCloudRenderer->RestoreDeviceObjects(NULL);
		if(simulLightningRenderer)
			simulLightningRenderer->RestoreDeviceObjects();
	}
	ConnectInterfaces();
}

SimulGLWeatherRenderer::~SimulGLWeatherRenderer()
{
	InvalidateDeviceObjects();
	operator delete(simulSkyRenderer,memoryInterface);
	operator delete(simulCloudRenderer,memoryInterface);
	operator delete(simul2DCloudRenderer,memoryInterface);
	operator delete(simulLightningRenderer,memoryInterface);
	operator delete(simulPrecipitationRenderer,memoryInterface);
	operator delete(simulAtmosphericsRenderer,memoryInterface);
}

void SimulGLWeatherRenderer::SetScreenSize(int w,int h)
{
	BufferWidth=w/Downscale;
	BufferHeight=h/Downscale;
    if(scene_buffer)
	{
		delete scene_buffer;
		baseFramebuffer=scene_buffer=new FramebufferGL(BufferWidth,BufferHeight,GL_TEXTURE_2D);
		
		scene_buffer->InitColor_Tex(0,internal_buffer_format);
	}
}

void SimulGLWeatherRenderer::RestoreDeviceObjects(void*)
{
	glewInit();
	if(!CheckExtension("GL_VERSION_2_0"))
		throw simul::base::RuntimeError("OpenGL version 2.0 is not supported on this hardware");
	CheckExtension("GL_ARB_fragment_program");
	CheckExtension("GL_ARB_vertex_program");
	CheckExtension("GL_ARB_texture_float");
	CheckExtension("GL_ARB_color_buffer_float");
	CheckExtension("GL_EXT_framebuffer_object");
    if(scene_buffer)
        delete scene_buffer;
ERROR_CHECK
	baseFramebuffer=scene_buffer=new FramebufferGL(BufferWidth,BufferHeight,GL_TEXTURE_2D);
ERROR_CHECK
	scene_buffer->InitColor_Tex(0,internal_buffer_format);
ERROR_CHECK
	device_initialized=true;
	EnableCloudLayers();
	///simulSkyRenderer->RestoreDeviceObjects();
	//simulCloudRenderer->RestoreDeviceObjects(NULL);
	//simulLightningRenderer->RestoreDeviceObjects();
	simulAtmosphericsRenderer->RestoreDeviceObjects(NULL);
	SAFE_DELETE_PROGRAM(cloud_overlay_program);
	cloud_overlay_program=MakeProgram("simple.vert",NULL,"simul_cloud_overlay.frag");
}
void SimulGLWeatherRenderer::InvalidateDeviceObjects()
{
	if(simulSkyRenderer)
		simulSkyRenderer->InvalidateDeviceObjects();
	if(simulCloudRenderer)
		simulCloudRenderer->InvalidateDeviceObjects();
	if(simulLightningRenderer)
		simulLightningRenderer->InvalidateDeviceObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->InvalidateDeviceObjects();
	if(scene_buffer)
		scene_buffer->InvalidateDeviceObjects();
	SAFE_DELETE_PROGRAM(cloud_overlay_program);
}

void SimulGLWeatherRenderer::ReloadTextures()
{
	BaseWeatherRenderer::ReloadTextures();
}

void SimulGLWeatherRenderer::RecompileShaders()
{
	BaseWeatherRenderer::RecompileShaders();
	SAFE_DELETE_PROGRAM(cloud_overlay_program);
	std::map<std::string,std::string> defines;
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	cloud_overlay_program=MakeProgram("simple.vert",NULL,"simul_cloud_overlay.frag",defines);
}

bool SimulGLWeatherRenderer::RenderSky(void *context,float exposure,bool buffered,bool is_cubemap)
{
	buffered&=(Utilities::GetSingleton().simple_program>0);
ERROR_CHECK
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
ERROR_CHECK
	BaseWeatherRenderer::RenderSky(context,exposure,buffered,is_cubemap);
	if(buffered)
	{
		glUseProgram(Utilities::GetSingleton().simple_program);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_SRC_ALPHA);
		scene_buffer->Render(context,true);
		glUseProgram(0);
	}
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
	int d=0;
	glGetIntegerv(GL_ATTRIB_STACK_DEPTH,&d);
	glPopAttrib();
	return true;
}

void SimulGLWeatherRenderer::RenderSkyAsOverlay(void *context,float exposure,bool buffered,bool is_cubemap,const void* depthTexture,int viewport_id,const simul::sky::float4& relativeViewportTextureRegionXYWH)
{
//	BaseWeatherRenderer::RenderSkyAsOverlay(context,buffered,is_cubemap,depthTexture);
	RenderCloudsLate=false;
#if 1
	if(baseSkyRenderer)
	{
		baseSkyRenderer->EnsureTexturesAreUpToDate(context);
		baseSkyRenderer->Render2DFades(context);
	}
	buffered=(buffered&&baseFramebuffer&&!is_cubemap);
	UpdateSkyAndCloudHookup();
	if(baseAtmosphericsRenderer&&ShowSky)
		baseAtmosphericsRenderer->RenderAsOverlay(context, depthTexture,exposure,relativeViewportTextureRegionXYWH);
	if(base2DCloudRenderer&&base2DCloudRenderer->GetCloudKeyframer()->GetVisible())
		base2DCloudRenderer->Render(context,exposure,false,depthTexture,UseDefaultFog,false,viewport_id,relativeViewportTextureRegionXYWH);
	if(buffered)
	{
		baseFramebuffer->Activate(context);
		baseFramebuffer->Clear(context,0.0f,0.0f,0.f,1.f,ReverseDepth?0.f:1.f);
	}
	if(baseSkyRenderer)
	{
		float cloud_occlusion=0;
		if(baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
			cloud_occlusion=baseCloudRenderer->GetSunOcclusion();
		baseSkyRenderer->CalcSunOcclusion(cloud_occlusion);
	}
	// Do this AFTER sky render, to catch any changes to texture definitions:
	UpdateSkyAndCloudHookup();
	if(baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
		baseCloudRenderer->Render(context,buffered?1.f:exposure,is_cubemap,depthTexture,UseDefaultFog,true,viewport_id,relativeViewportTextureRegionXYWH);
	if(buffered)
	{
		baseFramebuffer->Deactivate(context);
	//	setTexture(Utilities::GetSingleton().simple_program,"image_texure",0,(GLuint)baseFramebuffer->GetColorTex());
		glUseProgram(Utilities::GetSingleton().simple_program);
		//setParameter(
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_SRC_ALPHA);
		scene_buffer->Render(context,true);
		glUseProgram(0);
	}
#endif
}

void SimulGLWeatherRenderer::RenderLateCloudLayer(void *context,float exposure,bool,int viewport_id,const simul::sky::float4 &relativeViewportTextureRegionXYWH)
{
	if(!(AlwaysRenderCloudsLate||RenderCloudsLate)||!simulCloudRenderer||!simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		return;
	static simul::base::Timer timer;
	timer.TimeSum=0;
	timer.StartTime();
	
	scene_buffer->Activate(context);
	scene_buffer->Clear(context,0,0,0,1.f,ReverseDepth?0.f:1.f);
	simulCloudRenderer->Render(context,exposure,false,NULL,UseDefaultFog,true,viewport_id,relativeViewportTextureRegionXYWH);
	scene_buffer->Deactivate(context);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glBlendFunc(GL_ONE,GL_SRC_ALPHA);
	GLuint prog=cloud_overlay_program;//AlwaysRenderCloudsLate?cloud_overlay_program:Utilities::GetSingleton().simple_program;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,(GLuint)scene_buffer->GetColorTex());
	glUseProgram(prog);

	GLint image_texture		=glGetUniformLocation(prog,"image_texture");
	GLint depthAlphaTexture	=glGetUniformLocation(prog,"depthAlphaTexture");
	glUniform1i(image_texture,0);
	glUniform1i(depthAlphaTexture,1);
	scene_buffer->Render(context,true);
	glUseProgram(0);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	
	timer.FinishTime();
	gpu_time+=timer.Time;
}

void SimulGLWeatherRenderer::RenderLightning(void *context,int viewport_id)
{
	if(simulCloudRenderer&&simulLightningRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		simulLightningRenderer->Render(context);
}

void SimulGLWeatherRenderer::RenderPrecipitation(void *context)
{
	if(simulPrecipitationRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render(context);
}


class SimulGLSkyRenderer *SimulGLWeatherRenderer::GetSkyRenderer()
{
	return simulSkyRenderer;
}

class SimulGLCloudRenderer *SimulGLWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer;
}

class SimulGL2DCloudRenderer *SimulGLWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer;
}
/*
const char *SimulGLWeatherRenderer::GetDebugText() const
{
	static char debug_text[256];
	sprintf_s(debug_text,256,"RENDER %3.3g ms (clouds %3.3g ms, sky %3.3g ms, final %3.3g)\r\n"
		"TEXTURE UPDATE %3.3g ms (sky %3.3g ms)\r\n"
		"UPDATE %3.3g ms (clouds %3.3g ms, sky %3.3g ms)",
			total_timing
			,baseCloudRenderer?baseCloudRenderer->GetRenderTime():0.f
			,baseSkyRenderer?baseSkyRenderer->GetRenderTime():0.f
			,render_time
			,baseSkyRenderer?baseSkyRenderer->GetTextureUpdateTime():0.f
			,baseSkyRenderer?baseSkyRenderer->GetTextureUpdateTime():0.f
			,environment->total_update_timing
			,environment->cloud_update_timing
			,environment->sky_update_timing);
	return debug_text;
}
*/
GLuint SimulGLWeatherRenderer::GetFramebufferTexture()
{
	return (GLuint)scene_buffer->GetColorTex();
}