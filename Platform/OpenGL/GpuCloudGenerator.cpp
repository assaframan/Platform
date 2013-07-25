#include <gl/glew.h>
#include "Simul/Platform/OpenGL/GpuCloudGenerator.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/simul_gpu_clouds.sl"
using namespace simul::opengl;
/*
static void MakeVertexMatrix(const int *grid,int start_texel,int texels)
{
	int gridsize=grid[0]*grid[1]*grid[2];
	//start_texel-=grid[0]*grid[1];
	if(start_texel<0)
		start_texel=0;
	//texels+=grid[0]*grid[1];
	if(start_texel+texels>gridsize)
		texels=start_texel-gridsize;
	float y_start=(float)start_texel/(float)gridsize;
	float y_end=(float)(start_texel+texels)/(float)gridsize;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,1.0,y_start,y_end,-1.0,1.0);
}*/

GpuCloudGenerator::GpuCloudGenerator():BaseGpuCloudGenerator()
			,density_program(0)
			,lighting_program(0)
			,transform_program(0)
			,density(NULL)
			,density_gridsize(0)
			,readback_to_cpu(true)
			,gpuCloudConstantsUBO(0)
			,gpuCloudConstantsBindingIndex(2)
{
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	delete [] density;
}

void GpuCloudGenerator::RestoreDeviceObjects(void *)
{
	iformat=GL_LUMINANCE32F_ARB;
	itype=GL_LUMINANCE;
	RecompileShaders();
	density_texture=0;
	MAKE_CONSTANT_BUFFER(gpuCloudConstantsUBO,GpuCloudConstants,gpuCloudConstantsBindingIndex);
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	for(int i=0;i<2;i++)
	{
		fb[i].InvalidateDeviceObjects();
	}
	SAFE_DELETE_PROGRAM(density_program);
	SAFE_DELETE_PROGRAM(transform_program);
	SAFE_DELETE_PROGRAM(lighting_program);
	SAFE_DELETE_TEXTURE(density_texture);
	SAFE_DELETE_BUFFER(gpuCloudConstantsUBO);
}

void GpuCloudGenerator::RecompileShaders()
{
	SAFE_DELETE_PROGRAM(density_program);
	SAFE_DELETE_PROGRAM(transform_program);
	SAFE_DELETE_PROGRAM(lighting_program);
	density_program		=MakeProgram("simul_gpu_clouds.vert",NULL,"simul_gpu_cloud_density.frag");
	lighting_program	=MakeProgram("simul_gpu_clouds.vert",NULL,"simul_gpu_clouds.frag");
	transform_program	=MakeProgram("simul_gpu_clouds.vert",NULL,"simul_gpu_cloud_transform.frag");
}

static GLuint make3DTexture(int w,int l,int d,int stride,bool wrap_z,const float *src)
{
	GLuint tex=0;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_3D,tex);
	glTexImage3D(GL_TEXTURE_3D,0,stride==1?GL_LUMINANCE32F_ARB:GL_RGBA32F_ARB,w,l,d,0,stride==1?GL_LUMINANCE:GL_RGBA,GL_FLOAT,src);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);//GL_LINEAR_MIPMAP_LINEAR
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,wrap_z?GL_REPEAT:GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_3D,0);
	return tex;
}

int GpuCloudGenerator::GetDensityGridsize(const int *grid)
{
	if(iformat==GL_LUMINANCE32F_ARB)
	{
		dens_fb.SetWidthAndHeight(grid[0],grid[1]*grid[2]);
		if(!dens_fb.InitColor_Tex(0,iformat))
		{
			itype=GL_INTENSITY;
			iformat=GL_INTENSITY32F_ARB;
			if(!dens_fb.InitColor_Tex(0,iformat))
			{
				itype=GL_RGBA;
				iformat=GL_RGBA32F_ARB;
				dens_fb.InitColor_Tex(0,iformat);
			}
		}
	}
	return grid[0]*grid[1]*grid[2];
}

void* GpuCloudGenerator::Make3DNoiseTexture(int noise_size,const float *noise_src_ptr)
{
	return (void*)make3DTexture(noise_size,noise_size,noise_size,1,true,noise_src_ptr);
}

void GpuCloudGenerator::CycleTexturesForward()
{
}

// Fill the stated number of texels of the density texture
void GpuCloudGenerator::FillDensityGrid(int index,const int *density_grid
											,int start_texel
											,int texels
											,float humidity
											,float baseLayer
											,float transition
											,float upperDensity
											,float time
											,void* noise_tex,int octaves,float persistence
											,bool mask)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
simul::base::Timer timer;
timer.StartTime();
	int total_texels=GetDensityGridsize(density_grid);
	if(!density_program)
		RecompileShaders();
	// We render out a 2D texture with each XY layer laid end-to-end, and copy it to the target.
	dens_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	if(!dens_fb.InitColor_Tex(0,iformat))
	{
		itype=GL_INTENSITY;
		iformat=GL_INTENSITY32F_ARB;
		if(!dens_fb.InitColor_Tex(0,iformat))
		{
			itype=GL_RGBA;
			iformat=GL_RGBA32F_ARB;
			dens_fb.InitColor_Tex(0,iformat);
		}
	}
	int stride=(iformat==GL_RGBA32F_ARB)?4:1;

	simul::math::Vector3 noise_scale(1.f,1.f,(float)density_grid[2]/(float)density_grid[0]);

	//using noise_size and noise_src_ptr, make a 3d texture:
	GLuint volume_noise_tex=(GLuint)noise_tex;
	
	glEnable(GL_TEXTURE_3D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,volume_noise_tex);
	
	glUseProgram(density_program);
	
	setParameter(density_program,"volumeNoiseTexture"	,0);

	GpuCloudConstants constants;
	constants.octaves		=octaves;
	constants.persistence	=persistence;
	constants.humidity		=humidity;
	constants.time			=time;
	constants.noiseScale	=noise_scale;

	constants.zPosition		=0.f;
	//constants.vertexMatrix	=vertexMatrix;
	constants.extinctions	;
	constants.zPixel			=1.f/(float)density_grid[2];
	constants.zSize				=(float)density_grid[2];
	constants.baseLayer			=baseLayer;
	constants.transition		=transition;
	constants.upperDensity		=upperDensity;

	UPDATE_CONSTANT_BUFFER(gpuCloudConstantsUBO,constants,gpuCloudConstantsBindingIndex)
	GLint gpuCloudConstants		=glGetUniformBlockIndex(density_program,"GpuCloudConstants");
	if(gpuCloudConstants>=0)
		glUniformBlockBinding(density_program,gpuCloudConstants,gpuCloudConstantsBindingIndex);
	//MakeVertexMatrix(density_grid,start_texel,texels);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,1.0,0.0,1.0,-1.0,1.0);
	float y_start=(float)start_texel/(float)total_texels;
	float y_end=(float)(start_texel+texels)/(float)total_texels;

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	{
ERROR_CHECK
		dens_fb.Activate(NULL);
//dens_fb.Clear(0,0,0,0);
ERROR_CHECK
		DrawQuad(0.f,y_start,1.f,y_end-y_start);
std::cout<<"\tGpu clouds: DrawQuad "<<timer.UpdateTime()<<std::endl;
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,density_texture);

		int w,l,d;
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_WIDTH,&w);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_HEIGHT,&l);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_DEPTH,&d);

		if(!density_texture||density_gridsize!=total_texels*stride||w!=density_grid[0]||l!=density_grid[1]||d!=density_grid[2])
		{
			SAFE_DELETE_TEXTURE(density_texture);
			glBindTexture(GL_TEXTURE_3D,0);
			if(readback_to_cpu&&total_texels*stride>density_gridsize)
			{
				density_gridsize=total_texels*stride;
			}
			density_texture	=make3DTexture(density_grid[0],density_grid[1],density_grid[2],iformat==GL_RGBA32F_ARB?4:1,false,NULL);
			glBindTexture(GL_TEXTURE_3D,density_texture);
		}
		glEnable(GL_TEXTURE_3D);
	ERROR_CHECK
		{
			// Now instead of reading the pixels back to memory, we will copy them layer-by-layer into the volume texture.
			int Y=start_texel/density_grid[0];
			int H=texels/density_grid[0];
			int z0=Y/density_grid[1];
			int z1=(start_texel+texels)/density_grid[0]/density_grid[1];
			int y0=Y-z0*density_grid[1];
			int y1=Y+H-(z1-1)*density_grid[1];
			for(int i=z0;i<z1;i++)
			{
				int y=0,dy=density_grid[1];
				if(i==z0)
				{
					y=y0;
					dy=density_grid[1]-y0;
				}
				if(i==z1-1)
				{
					dy=y1-y;
				}
		ERROR_CHECK
				glCopyTexSubImage3D(	GL_TEXTURE_3D,
 										0,						//	level
 										0,						//	x offset in 3D texture
 										y,						//	y offset in 3D texture
 										i,						//	z offset in 3D texture
 										0,						//	x=0 in source 2D texture
 										i*density_grid[1]+y,	//	y offset in source 2D texture
 										density_grid[0],	
 										dy);
		ERROR_CHECK
 			}
		}
		dens_fb.Deactivate(NULL);
	}
	glDisable(GL_TEXTURE_3D);
	glUseProgram(0);
ERROR_CHECK
std::cout<<"\tGpu clouds: glReadPixels "<<timer.UpdateTime()<<std::endl;
	glDeleteTextures(1,&volume_noise_tex);
ERROR_CHECK
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

// The target is a grid of size given by light_gridsizes, arranged as d w-by-l textures.
void GpuCloudGenerator::PerformGPURelight(int index,float *target
										,const int *light_grid
										,int start_texel
										,int texels
										,const int * //density_grid
										,const float *transformMatrix
										,const float *lightspace_extinctions_float3)
{
ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
simul::base::Timer timer;
timer.StartTime();
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight(light_grid[0],light_grid[1]);
		fb[i].InitColor_Tex(0,GL_RGBA32F_ARB);
	}
	// We use the density framebuffer texture as our density texture. This works well
	// because we don't need to do any filtering.
	//GLuint density_texture	=dens_fb.GetColorTex();
	// blit from dens_fb...
	glUseProgram(lighting_program);
	setParameter(lighting_program,"input_light_texture",0);
	setParameter(lighting_program,"density_texture",1);

	GpuCloudConstants constants;
	constants.extinctions=lightspace_extinctions_float3;
	constants.transformMatrix=transformMatrix;
	UPDATE_CONSTANT_BUFFER(gpuCloudConstantsUBO,constants,gpuCloudConstantsBindingIndex)
	GLint gpuCloudConstants		=glGetUniformBlockIndex(lighting_program,"GpuCloudConstants");
	if(gpuCloudConstants>=0)
		glUniformBlockBinding(lighting_program,gpuCloudConstants,gpuCloudConstantsBindingIndex);
	// initialize the first input texture.
	FramebufferGL *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_3D);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,density_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetColorTex());
	ERROR_CHECK
	glBindTexture(GL_TEXTURE_2D,0);
	ERROR_CHECK
	int light_gridsize=light_grid[0]*light_grid[1]*light_grid[2];
	int z0=(start_texel*light_grid[2])/light_gridsize;
	int z1=((start_texel+texels)*light_grid[2])/light_gridsize;
	if(z0%2)
	{
		std::swap(F[0],F[1]);
	}
	if(z0==0)
	{
		F[0]->Activate(NULL);
			F[0]->Clear(NULL,1.f,1.f,1.f,1.f,1.f);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glReadPixels(0,0,light_grid[0],light_grid[1],GL_RGBA,GL_FLOAT,(GLvoid*)target);
		F[0]->Deactivate(NULL);
		z0++;
	}
	target+=z0*light_grid[0]*light_grid[1]*4;
ERROR_CHECK
	float draw_time=0.f,read_time=0.f;
	for(int i=z0;i<z1;i++)
	{
		float zPosition=((float)i-0.5f)/(float)light_grid[2];
		//setParameter(lighting_program,"zPosition",zPosition);
		constants.zPosition=zPosition;
		UPDATE_CONSTANT_BUFFER(gpuCloudConstantsUBO,constants,gpuCloudConstantsBindingIndex)
		GLint gpuCloudConstants		=glGetUniformBlockIndex(lighting_program,"GpuCloudConstants");
		if(gpuCloudConstants>=0)
			glUniformBlockBinding(lighting_program,gpuCloudConstants,gpuCloudConstantsBindingIndex);
		F[1]->Activate(NULL);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0,1.0,0,1.0,-1.0,1.0);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			// input light values:
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetColorTex());
			DrawQuad(0,0,1,1);
			draw_time+=timer.UpdateTime();
			ERROR_CHECK
		// Copy F[1] contents to the target
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glReadPixels(0,0,light_grid[0],light_grid[1],GL_RGBA,GL_FLOAT,(GLvoid*)target);
			read_time+=timer.UpdateTime();
			ERROR_CHECK
		F[1]->Deactivate(NULL);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		std::swap(F[0],F[1]);
		target+=light_grid[0]*light_grid[1]*4;
	}
	glUseProgram(0);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
std::cout<<"\tGpu clouds: DrawQuad "<<draw_time<<std::endl;
std::cout<<"\tGpu clouds: glReadPixels "<<read_time<<std::endl;
std::cout<<"\tGpu clouds: SAFE_DELETE_TEXTURE "<<timer.UpdateTime()<<std::endl;
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

// Transform light data into a world-oriented cloud texture.
// The inputs are in RGBA float32 format, with the light values in the RG slots.
void GpuCloudGenerator::GPUTransferDataToTexture(int index,unsigned char *target
											,const float *transformMatrix
											,const float *light,const int *light_grid
											,const float *ambient,const int *density_grid
											,int start_texel
											,int texels)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	int total_texels=density_grid[0]*density_grid[1]*density_grid[2];
	// For each level in the z direction, we render out a 2D texture and copy it to the target.
	world_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	world_fb.InitColor_Tex(0,GL_RGBA);
	//GLuint density_texture	=make3DTexture(density_grid[0],density_grid[1],density_grid[2]	,1,false,density);
	GLuint light_texture	=make3DTexture(light_grid[0]	,light_grid[1],light_grid[2]	,4,false,light);
	GLuint ambient_texture	=make3DTexture(density_grid[0],density_grid[1],density_grid[2]	,4,false,ambient);
			glUseProgram(transform_program);
	setParameter(transform_program,"density_texture",0);
	setParameter(transform_program,"light_texture",1);
	setParameter(transform_program,"ambient_texture",2);
	//setMatrix(transform_program,"transformMatrix",transformMatrix);
//	setParameter(transform_program,"zSize",(float)density_grid[2]);
	//setParameter(transform_program,"zPixel",1.f/(float)density_grid[2]);

	GpuCloudConstants constants;
	constants.transformMatrix	=transformMatrix;
	constants.zPixel			=1.f/(float)density_grid[2];
	constants.zSize				=(float)density_grid[2];
	//constants.zs
	UPDATE_CONSTANT_BUFFER(gpuCloudConstantsUBO,constants,gpuCloudConstantsBindingIndex)
	GLint gpuCloudConstants		=glGetUniformBlockIndex(transform_program,"GpuCloudConstants");
	if(gpuCloudConstants>=0)
		glUniformBlockBinding(transform_program,gpuCloudConstants,gpuCloudConstantsBindingIndex);
	glEnable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,density_texture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,light_texture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_3D,ambient_texture);
	float y_start=(float)start_texel/(float)total_texels;
	float y_end=(float)(start_texel+texels)/(float)total_texels;
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.
	{
		world_fb.Activate(NULL);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0,1.0,0,1.0,-1.0,1.0);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			// input light values:
			DrawQuad(0.f,y_start,1.f,y_end-y_start);
			ERROR_CHECK
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			ERROR_CHECK
			int YMAX=density_grid[1]*density_grid[2];
			int Y0=(int)(y_start*(float)YMAX);
			int Y1=(int)(y_end*(float)YMAX);
			target+=Y0*density_grid[0]*4;
		glReadPixels(0,Y0,density_grid[0],Y1-Y0,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,(GLvoid*)target);
			ERROR_CHECK
		world_fb.Deactivate(NULL);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			ERROR_CHECK
		//target+=density_grid[0]*density_grid[1]*4;
	}
	glUseProgram(0);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_TEXTURE_2D);
	glDeleteTextures(1,&ambient_texture);
	//glDeleteTextures(1,&density_texture);
	glDeleteTextures(1,&light_texture);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}