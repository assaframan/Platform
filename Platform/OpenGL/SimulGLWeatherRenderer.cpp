
#ifdef _MSC_VER
#include <GL/glew.h>
#endif

#include "FramebufferGL.h"
#include "LoadGLProgram.h"
#include "SimulGLWeatherRenderer.h"
#include "SimulGLSkyRenderer.h"
#include "SimulGLCloudRenderer.h"
#include "SimulGL2DCloudRenderer.h"
#include "SimulGLLightningRenderer.h"
#include "SimulGLUtilities.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/Decay.h"

#ifdef _MSC_VER
// for wglGetProcAddress
#include <Windows.h>
GLenum buffer_tex_format=GL_FLOAT;//GL_HALF_FLOAT_NV;
GLenum internal_buffer_format=GL_RGBA32F_ARB;//GL_RGBA16F_ARB;
#else
GLenum buffer_tex_format=GL_FLOAT;
GLenum internal_buffer_format=GL_RGBA32F_ARB;
#endif

SimulGLWeatherRenderer::SimulGLWeatherRenderer(const char *lic,simul::clouds::CloudKeyframer *cloudKeyframer,bool usebuffer,bool tonemap,int width,
		int height,bool sky,bool clouds3d,bool clouds2d,bool rain,bool colour_sky)
		:BaseWeatherRenderer(lic)
		,BufferWidth(width)
		,BufferHeight(height)
		,device_initialized(false)
		,scene_buffer(NULL)
{
	if(sky)
	{
		simulSkyRenderer=new SimulGLSkyRenderer(colour_sky);
		baseSkyRenderer=simulSkyRenderer.get();
	}
	if(simulSkyRenderer)
	{
		simulSkyRenderer->Create(0.5f);
	}
	simulCloudRenderer=new SimulGLCloudRenderer(license_key,cloudKeyframer);
	baseCloudRenderer=simulCloudRenderer.get();
	
	simulLightningRenderer=new SimulGLLightningRenderer(simulCloudRenderer->GetCloudKeyframer()->GetLightningRenderInterface());
	baseLightningRenderer=simulLightningRenderer.get();
	
	EnableCloudLayers(clouds3d,clouds2d);
}


void SimulGLWeatherRenderer::EnableCloudLayers(bool clouds3d,bool clouds2d)
{
	simul::clouds::BaseWeatherRenderer::EnableCloudLayers(clouds3d,clouds2d);
	//if(clouds2d)
	//	base2DCloudRenderer=simul2DCloudRenderer=new SimulGL2DCloudRenderer();
	if(simulSkyRenderer)
	{
		if(device_initialized)
			simulSkyRenderer->RestoreDeviceObjects();
	}
/*	if(clouds2d&&!simul2DCloudRenderer.get())
	{	
		simul2DCloudRenderer=new SimulGL2DCloudRenderer();
		base2DCloudRenderer=simul2DCloudRenderer.get();
	}*/
	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetBaseSkyInterface());
		simul2DCloudRenderer->Create();
		if(device_initialized)
			simul2DCloudRenderer->RestoreDeviceObjects(NULL);
	}
	if(simulCloudRenderer)
	{
		simulCloudRenderer->Create();
		if(device_initialized)
			simulCloudRenderer->RestoreDeviceObjects(NULL);
	}
	if(simulLightningRenderer)
	{
		if(device_initialized)
			simulLightningRenderer->RestoreDeviceObjects();
	}
	ConnectInterfaces();
}

SimulGLWeatherRenderer::~SimulGLWeatherRenderer()
{
}

bool SimulGLWeatherRenderer::RestoreDeviceObjects()
{
	GLenum res=glewInit();
	const char* extensionsString = (const char*)glGetString(GL_EXTENSIONS);
// If the GL_GREMEDY_string_marker extension is supported:
	if(glewIsSupported("GL_GREMEDY_string_marker"))
	{
		// Get a pointer to the glStringMarkerGREMEDY function:
		glStringMarkerGREMEDY = (PFNGLSTRINGMARKERGREMEDYPROC)wglGetProcAddress("glStringMarkerGREMEDY");
	}
	CheckGLError(__FILE__,__LINE__,res);
	if(!GLEW_VERSION_2_0)
	{
		std::cerr<<"GL ERROR: No OpenGL 2.0 support on this hardware!\n";
	}
//	CheckExtension("GL_VERSION_2_0");
	CheckExtension("GL_ARB_fragment_program");
	CheckExtension("GL_ARB_vertex_program");
	CheckExtension("GL_ARB_texture_float");
	CheckExtension("GL_ARB_color_buffer_float");
	CheckExtension("GL_EXT_framebuffer_object");
    if(scene_buffer)
        delete scene_buffer;
	scene_buffer=new FramebufferGL(BufferWidth,BufferHeight,GL_TEXTURE_2D);
	scene_buffer->InitColor_Tex(0,internal_buffer_format,buffer_tex_format);
	scene_buffer->SetShader(0);
	device_initialized=true;
	EnableCloudLayers(simulCloudRenderer.get()!=NULL,simul2DCloudRenderer.get()!=NULL);
	///simulSkyRenderer->RestoreDeviceObjects();
	//simulCloudRenderer->RestoreDeviceObjects(NULL);
	//simulLightningRenderer->RestoreDeviceObjects();
	return true;
}
bool SimulGLWeatherRenderer::InvalidateDeviceObjects()
{
	return true;
}

bool SimulGLWeatherRenderer::RenderSky(bool buffered,bool is_cubemap)
{
	ERROR_CHECK
static simul::base::Timer timer;
	timer.TimeSum=0;
	timer.StartTime();
	BaseWeatherRenderer::RenderSky(buffered,is_cubemap);
	bool hr=false;
	glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	if(simulSkyRenderer)
	{
		ERROR_CHECK
		//simulSkyRenderer->RenderPointStars();
		ERROR_CHECK
		simulSkyRenderer->RenderPlanets();
		ERROR_CHECK
		simulSkyRenderer->RenderSun();
		ERROR_CHECK
	}
	// Everything between Activate() and DeactivateAndRender() is drawn to the buffer object.
	if(buffered)
	{
		scene_buffer->Activate();
		glClearColor(0.f,0.f,0.f,1.f);
		ERROR_CHECK
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		ERROR_CHECK
	}
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	ERROR_CHECK
	if(simulSkyRenderer)
	{
		// We call the sky renderer, telling it to blend if we're not buffering the sky,
		// because otherwise it would overwrite the planets
		simulSkyRenderer->Render(!buffered);
	}
	ERROR_CHECK
	timer.UpdateTime();
	simul::math::FirstOrderDecay(sky_timing,timer.Time,0.1f,0.01f);
	ERROR_CHECK
    if(simul2DCloudRenderer)
		simul2DCloudRenderer->Render(false,false,false);
	ERROR_CHECK
	if(simulCloudRenderer&&layer1)
		simulCloudRenderer->Render(false,false,UseDefaultFog);
	timer.UpdateTime();
	simul::math::FirstOrderDecay(cloud_timing,timer.Time,0.1f,0.01f);
	if(buffered)
		scene_buffer->DeactivateAndRender(true);
	ERROR_CHECK
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
	ERROR_CHECK
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
	ERROR_CHECK
	int d=0;
	glGetIntegerv(GL_ATTRIB_STACK_DEPTH,&d);
	glPopAttrib();
	ERROR_CHECK
	timer.FinishTime();
	simul::math::FirstOrderDecay(final_timing,timer.Time,0.1f,0.01f);
	simul::math::FirstOrderDecay(total_timing,timer.TimeSum,0.1f,0.01f);
	return true;
}

void SimulGLWeatherRenderer::RenderLightning()
{
	if(simulCloudRenderer&&simulLightningRenderer&&layer1)
		simulLightningRenderer->Render();
}


void SimulGLWeatherRenderer::RenderClouds(bool buffered,bool depth_testing,bool default_fog)
{
static simul::base::Timer timer;
	timer.StartTime();
	ERROR_CHECK
	glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	if(buffered)
	{
		scene_buffer->Activate();
		glClearColor(0.f,0.f,0.f,1.f);
		ERROR_CHECK
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		ERROR_CHECK
	}
    if(simulCloudRenderer)
		simulCloudRenderer->Render(false,depth_testing,default_fog);
	ERROR_CHECK

	if(buffered)
		scene_buffer->DeactivateAndRender(true);
	ERROR_CHECK
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
	glPopAttrib();
	ERROR_CHECK
	timer.FinishTime();
	simul::math::FirstOrderDecay(cloud_timing,timer.Time,0.1f,0.01f);
}


class SimulGLSkyRenderer *SimulGLWeatherRenderer::GetSkyRenderer()
{
	return simulSkyRenderer.get();
}

class SimulGLCloudRenderer *SimulGLWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer.get();
}

class SimulGL2DCloudRenderer *SimulGLWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer.get();
}

static void writechar(std::ostream &os,char c)
{
	os.write((const char*)&c,sizeof(c));
}
static char readchar(std::istream &is)
{
	char c;
	is.read((char*)&c,sizeof(c));
	return c;
}

std::ostream &SimulGLWeatherRenderer::Save(std::ostream &os) const
{
	//\211   P   N   G  \r  \n \032 \n
	writechar(os,-45);
	writechar(os,'S');
	writechar(os,'E');
	writechar(os,'Q');
	writechar(os,'\r');
	writechar(os,'\n');
	writechar(os,32);
	int num_streams=2;
	os.write((const char*)&num_streams,sizeof(num_streams));
	int stream_type=simulSkyRenderer->IsColourSkyEnabled()?2:1;
	os.write((const char*)&stream_type,sizeof(stream_type));
	simulSkyRenderer->Save(os);
	stream_type=0;
	os.write((const char*)&stream_type,sizeof(stream_type));
	simulCloudRenderer->Save(os);
	return os;
}

std::istream &SimulGLWeatherRenderer::Load(std::istream &is)
{
	if(readchar(is)!=-45) return is;
	if(readchar(is)!='S') return is;
	if(readchar(is)!='E') return is;
	if(readchar(is)!='Q') return is;
	if(readchar(is)!='\r') return is;
	if(readchar(is)!='\n') return is;
	if(readchar(is)!=32) return is;
	if(readchar(is)!='\n') return is;
	int num_streams=0;
	is.read((char*)&num_streams,sizeof(num_streams));
	for(int i=0;i<num_streams;i++)
	{
		int stream_type=-1;
		is.read((char*)&stream_type,sizeof(stream_type));
		if(stream_type==1||stream_type==2)
		{
			simulSkyRenderer->EnableColourSky(stream_type==2);
			simulSkyRenderer->Load(is);
		}
		if(stream_type==0)
		{
			simulCloudRenderer->Load(is);
		}
	}
	ConnectInterfaces();
	return is;
}


const char *SimulGLWeatherRenderer::GetDebugText() const
{
	static char debug_text[256];
	sprintf_s(debug_text,256,"RENDER %3.3g ms (clouds %3.3g ms, sky %3.3g ms, final %3.3g)\r\n"
		"UPDATE %3.3g ms (clouds %3.3g ms, sky %3.3g ms)",
			total_timing,cloud_timing,sky_timing,final_timing,
			total_update_timing,cloud_update_timing,sky_update_timing);
	return debug_text;
}
