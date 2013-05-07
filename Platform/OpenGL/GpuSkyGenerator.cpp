#include <gl/glew.h>
#include "Simul/Platform/OpenGL/GpuSkyGenerator.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Platform/OpenGL/Glsl.h"
#include "Simul/Platform/CrossPlatform/simul_gpu_sky.sl"
#include "Simul/Base/Timer.h"
#include <math.h>
using namespace simul;
using namespace opengl;

GpuSkyGenerator::GpuSkyGenerator()
	:loss_program(0)
	,insc_program(0)
	,skyl_program(0)
	,gpuSkyConstantsBindingIndex(5)
{
}

GpuSkyGenerator::~GpuSkyGenerator()
{
}

void GpuSkyGenerator::RestoreDeviceObjects(void *)
{
	RecompileShaders();
}

void GpuSkyGenerator::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(loss_program);
	SAFE_DELETE_PROGRAM(insc_program);
	SAFE_DELETE_PROGRAM(skyl_program);
	SAFE_DELETE_BUFFER(gpuSkyConstantsUBO);
}

void GpuSkyGenerator::RecompileShaders()
{																												SAFE_DELETE_PROGRAM(loss_program);
	SAFE_DELETE_PROGRAM(insc_program);
	SAFE_DELETE_PROGRAM(skyl_program);
	loss_program=MakeProgram("simple.vert",NULL,"simul_gpu_loss.frag");
ERROR_CHECK
	insc_program=MakeProgram("simple.vert",NULL,"simul_gpu_insc.frag","#define OVERCAST 1\r\n");
ERROR_CHECK
	skyl_program=MakeProgram("simple.vert",NULL,"simul_gpu_skyl.frag");
ERROR_CHECK
	MAKE_CONSTANT_BUFFER(gpuSkyConstantsUBO,GpuSkyConstants,gpuSkyConstantsBindingIndex);
}

//! Return true if the derived class can make sky tables using the GPU.
bool GpuSkyGenerator::CanPerformGPUGeneration() const
{
	return Enabled;
}

// make a 3D texture. X=altitude (clamp), Y=elevation (clamp), Z=distance (clamp)
static GLuint make3DTexture(int w,int l,int d,const float *src)
{
	GLuint tex=0;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_3D,tex);
	glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F_ARB,w,l,d,0,GL_RGBA,GL_FLOAT,src);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);// no mipmaps fo not GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_3D,0);
	return tex;
}

// make a 1D texture. X=altitude (clamp)
static GLuint make1DTexture(int w,const float *src)
{
	GLuint tex=0;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_1D,tex);
	glTexImage1D(GL_TEXTURE_1D,0,GL_RGBA32F_ARB,w,0,GL_RGBA,GL_FLOAT,src);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_1D,0);
	return tex;
}


void GpuSkyGenerator::Make2DLossAndInscatterTextures(simul::sky::AtmosphericScatteringInterface *skyInterface
				,int numElevations,int numDistances
				,simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl
				,const std::vector<float> &altitudes_km
				,float max_distance_km
				,simul::sky::float4 sun_irradiance
				,simul::sky::float4 dir_to_sun,simul::sky::float4 dir_to_moon,float haze
				,float overcast,float overcast_base_km,float overcast_range_km
				,int index,int end_index
				,const simul::sky::float4 *density_table,const simul::sky::float4 *optical_table
		,const simul::sky::float4 *blackbody_table,int table_size,float maxDensityAltKm
				,bool InfraRed
				,float emissivity
				,float seaLevelTemperatureK)
{
	GLint gpuSkyConstants;
simul::base::Timer timer;
timer.StartTime();
	if(loss_program<=0)
		RecompileShaders();
std::cout<<"\tGpu sky: recompile "<<timer.UpdateTime()<<std::endl;
	float maxOutputAltKm=altitudes_km[altitudes_km.size()-1];
// we will render to these three textures, one distance at a time.
// The rendertextures are altitudes x elevations
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight(altitudes_km.size(),numElevations);
		fb[i].InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	}
std::cout<<"\tGpu sky: fb "<<timer.UpdateTime()<<std::endl;
	BaseFramebuffer *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	glEnable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	GLuint dens_tex=make1DTexture(table_size,(const float *)density_table);
std::cout<<"\tGpu sky: dens_tex "<<timer.UpdateTime()<<std::endl;
	glUseProgram(loss_program);
	{
		GpuSkyConstants constants;
		constants.texSize			=vec2((float)altitudes_km.size(),(float)numElevations);
		static float tto=0.5f;
		constants.texelOffset		=tto;
		constants.tableSize			=vec2((float)table_size,(float)table_size);
		constants.maxDistanceKm		=max_distance_km;
		constants.planetRadiusKm	=skyInterface->GetPlanetRadius();
		constants.maxOutputAltKm	=maxOutputAltKm;
		constants.maxDensityAltKm	=maxDensityAltKm;
		constants.hazeBaseHeightKm	=skyInterface->GetHazeBaseHeightKm();
		constants.hazeScaleHeightKm	=skyInterface->GetHazeScaleHeightKm();
		constants.overcastBaseKm	=overcast_base_km;
		constants.overcastRangeKm	=overcast_range_km;
		constants.overcast			=overcast;
		constants.rayleigh			=(const float*)skyInterface->GetRayleigh();
		constants.hazeMie			=(const float*)(haze*skyInterface->GetMie());
		constants.ozone				=(const float*)(skyInterface->GetOzoneStrength()*skyInterface->GetBaseOzone());
		constants.sunIrradiance		=(const float*)sun_irradiance;
		constants.lightDir			=(const float*)dir_to_sun;
		constants.starlight			=(const float*)(skyInterface->GetStarlight());
		constants.hazeEccentricity	=1.0;
		constants.mieRayleighRatio	=(const float*)(skyInterface->GetMieRayleighRatio());
		constants.emissivity		=emissivity;
		UPDATE_CONSTANT_BUFFER(gpuSkyConstantsUBO,constants,gpuSkyConstantsBindingIndex)
	}
	setParameter(loss_program,"input_loss_texture",0);
	setParameter(loss_program,"density_texture",1);
	gpuSkyConstants		=glGetUniformBlockIndex(loss_program,"GpuSkyConstants");
	if(gpuSkyConstants>=0)
		glUniformBlockBinding(loss_program,gpuSkyConstants,gpuSkyConstantsBindingIndex);


	simul::sky::float4 *target=loss;
ERROR_CHECK
	F[0]->Activate();
		F[0]->Clear(1.f,1.f,1.f,1.f);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glReadPixels(0,0,altitudes_km.size(),numElevations,GL_RGBA,GL_FLOAT,(GLvoid*)target);
	F[0]->Deactivate();
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	target+=altitudes_km.size()*numElevations;
ERROR_CHECK
	float prevDistKm=0.f;
	for(int i=1;i<numDistances;i++)
	{
	// The midpoint of the step represented by this layer
		float zPosition=pow((float)(i)/((float)numDistances-1.f),2.f);
		float distKm=zPosition*max_distance_km;
		if(i==numDistances-1)
			distKm=1000.f;
		setParameter(loss_program,"distKm"			,distKm);
		setParameter(loss_program,"prevDistKm"		,prevDistKm);
	ERROR_CHECK
		F[1]->Activate();
	ERROR_CHECK
			F[1]->Clear(0.f,0.f,0.f,0.f);
			OrthoMatrices();
			// input light values:
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetColorTex());
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_1D,dens_tex);
			DrawQuad(0,0,1,1);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
//std::cout<<"\tGpu sky: render loss"<<i<<" "<<timer.UpdateTime()<<std::endl;
	ERROR_CHECK
			glReadPixels(0,0,altitudes_km.size(),numElevations,GL_RGBA,GL_FLOAT,(GLvoid*)target);
//std::cout<<"\tGpu sky: loss read"<<i<<" "<<timer.UpdateTime()<<std::endl;
		F[1]->Deactivate();
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		std::swap(F[0],F[1]);
		target+=altitudes_km.size()*numElevations;
		prevDistKm=distKm;
	}
	glUseProgram(0);
std::cout<<"\tGpu sky: loss "<<timer.UpdateTime()<<std::endl;
	
	// Now we will generate the inscatter texture.
	// First we make the loss into a 3D texture.
	GLuint loss_tex=make3DTexture(altitudes_km.size(),numElevations,numDistances,(const float *)loss);
	GLuint optd_tex=make2DTexture(table_size,table_size,(const float *)optical_table);
std::cout<<"\tGpu sky: loss_tex,optd_tex "<<timer.UpdateTime()<<std::endl;
	// Now render out the inscatter.
	glUseProgram(insc_program);
	gpuSkyConstants		=glGetUniformBlockIndex(insc_program,"GpuSkyConstants");
	if(gpuSkyConstants>=0)
		glUniformBlockBinding(insc_program,gpuSkyConstants,gpuSkyConstantsBindingIndex);
	setParameter(insc_program,"input_insc_texture",0);
	setParameter(insc_program,"density_texture",1);
	setParameter(insc_program,"loss_texture",2);
	setParameter(insc_program,"optical_depth_texture",3);
ERROR_CHECK
	target=insc;
	F[0]->Activate();
		F[0]->Clear(0.f,0.f,0.f,0.f);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glReadPixels(0,0,altitudes_km.size(),numElevations,GL_RGBA,GL_FLOAT,(GLvoid*)target);
	F[0]->Deactivate();
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	target+=altitudes_km.size()*numElevations;
	prevDistKm=0.f;
	for(int i=1;i<numDistances;i++)
	{
	// The midpoint of the step represented by this layer
		float zPosition=pow((float)(i)/((float)numDistances-1.f),2.f);
		float distKm=zPosition*max_distance_km;
		if(i==numDistances-1)
			distKm=1000.f;
		setParameter(insc_program,"distKm"			,distKm);
		setParameter(insc_program,"prevDistKm"		,prevDistKm);
		F[1]->Activate();
			F[1]->Clear(0.f,0.f,0.f,0.f);
			OrthoMatrices();
			// input inscatter values:
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetColorTex());
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_1D,dens_tex);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_3D,loss_tex);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D,optd_tex);
			DrawQuad(0,0,1,1);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glReadPixels(0,0,altitudes_km.size(),numElevations,GL_RGBA,GL_FLOAT,(GLvoid*)target);
		F[1]->Deactivate();
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		std::swap(F[0],F[1]);
		target+=altitudes_km.size()*numElevations;
		prevDistKm=distKm;
	}
	glUseProgram(0);
std::cout<<"\tGpu sky: insc "<<timer.UpdateTime()<<std::endl;
	// Finally we will generate the skylight texture.
	// First we make the inscatter into a 3D texture.
	GLuint insc_tex=make3DTexture(altitudes_km.size(),numElevations,numDistances,(const float *)insc);
std::cout<<"\tGpu sky: insc_tex "<<timer.UpdateTime()<<std::endl;
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_3D,insc_tex);
	// Now render out the skylight.
	glUseProgram(skyl_program);
	gpuSkyConstants		=glGetUniformBlockIndex(skyl_program,"GpuSkyConstants");
	if(gpuSkyConstants>=0)
		glUniformBlockBinding(skyl_program,gpuSkyConstants,gpuSkyConstantsBindingIndex);
	setParameter(skyl_program,"input_skyl_texture",0);
	setParameter(skyl_program,"density_texture",1);
	setParameter(skyl_program,"loss_texture",2);
//	setParameter(skyl_program,"optical_depth_texture",3);
	setParameter(skyl_program,"insc_texture",4);
ERROR_CHECK
	target=skyl;
	F[0]->Activate();
		F[0]->Clear(0.f,0.f,0.f,0.f);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glReadPixels(0,0,altitudes_km.size(),numElevations,GL_RGBA,GL_FLOAT,(GLvoid*)target);
	F[0]->Deactivate();
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	target+=altitudes_km.size()*numElevations;
	prevDistKm=0.f;
	for(int i=1;i<numDistances;i++)
	{
	// The midpoint of the step represented by this layer
		float zPosition=pow((float)(i)/((float)numDistances-1.f),2.f);
		float distKm=zPosition*max_distance_km;
		if(i==numDistances-1)
			distKm=1000.f;
		setParameter(skyl_program,"distKm"			,distKm);
		setParameter(skyl_program,"prevDistKm"		,prevDistKm);
		F[1]->Activate();
			F[1]->Clear(0.f,0.f,0.f,0.f);
			OrthoMatrices();
			// input inscatter values:
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetColorTex());
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_1D,dens_tex);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_3D,loss_tex);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D,optd_tex);
			DrawQuad(0,0,1,1);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	ERROR_CHECK
			glReadPixels(0,0,altitudes_km.size(),numElevations,GL_RGBA,GL_FLOAT,(GLvoid*)target);
		F[1]->Deactivate();
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		std::swap(F[0],F[1]);
		target+=altitudes_km.size()*numElevations;
		prevDistKm=distKm;
	}
	glUseProgram(0);
std::cout<<"\tGpu sky: skylight "<<timer.UpdateTime()<<std::endl;
	SAFE_DELETE_TEXTURE(loss_tex);
	SAFE_DELETE_TEXTURE(dens_tex);
	SAFE_DELETE_TEXTURE(optd_tex);
	SAFE_DELETE_TEXTURE(insc_tex);
std::cout<<"\tGpu sky: SAFE_DELETE_TEXTURE "<<timer.UpdateTime()<<std::endl;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D,0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_3D,0);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_3D,0);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
std::cout<<"\tGpu sky: end "<<timer.UpdateTime()<<std::endl<<std::endl;
}