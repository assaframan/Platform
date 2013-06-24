#include "GpuCloudGenerator.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include <math.h>

using namespace simul;
using namespace dx11;

GpuCloudGenerator::GpuCloudGenerator()
			:m_pd3dDevice(NULL)
			,m_pImmediateContext(NULL)
			,effect(NULL)
			,densityTechnique(NULL)
			,lightingTechnique(NULL)
			,transformTechnique(NULL)
			,volume_noise_tex(NULL)
			,volume_noise_tex_srv(NULL)
			,density_texture(NULL)
			,density_texture_srv(NULL)
{
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	InvalidateDeviceObjects();
}

void GpuCloudGenerator::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	fb[0].RestoreDeviceObjects(m_pd3dDevice);
	fb[1].RestoreDeviceObjects(m_pd3dDevice);
	dens_fb.RestoreDeviceObjects(m_pd3dDevice);
	world_fb.RestoreDeviceObjects(m_pd3dDevice);
	gpuCloudConstants.RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	fb[0].InvalidateDeviceObjects();
	fb[1].InvalidateDeviceObjects();
	dens_fb.InvalidateDeviceObjects();
	world_fb.InvalidateDeviceObjects();
	SAFE_RELEASE(volume_noise_tex);
	SAFE_RELEASE(volume_noise_tex_srv);
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(effect);
	SAFE_RELEASE(density_texture_srv);
	SAFE_RELEASE(density_texture);
	gpuCloudConstants.InvalidateDeviceObjects();
	m_pd3dDevice=NULL;
}

void GpuCloudGenerator::RecompileShaders()
{
	SAFE_RELEASE(effect);
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,"simul_gpu_clouds.fx");
	if(effect)
	{
		densityTechnique	=effect->GetTechniqueByName("simul_gpu_density");
		lightingTechnique	=effect->GetTechniqueByName("simul_gpu_lighting");
		transformTechnique	=effect->GetTechniqueByName("simul_gpu_transform");
	}
	gpuCloudConstants.LinkToEffect(effect,"GpuCloudConstants");
}

int GpuCloudGenerator::GetDensityGridsize(const int *grid)
{
	//if(iformat==DXGI_FORMAT_R32G32B32A32_FLOAT)
	//	size=4;
	return grid[0]*grid[1]*grid[2];
}
void* GpuCloudGenerator::Make3DNoiseTexture(int noise_size,const float *noise_src_ptr)
{
	//using noise_size and noise_src_ptr, make a 3d texture:
	SAFE_RELEASE(volume_noise_tex);
	SAFE_RELEASE(volume_noise_tex_srv);
	volume_noise_tex=make3DTexture(m_pd3dDevice,noise_size,noise_size,noise_size,DXGI_FORMAT_R32_FLOAT,noise_src_ptr);

	m_pd3dDevice->CreateShaderResourceView(volume_noise_tex,NULL,&volume_noise_tex_srv);
	m_pImmediateContext->GenerateMips(volume_noise_tex_srv);
	return volume_noise_tex_srv;
}


void *GpuCloudGenerator::FillDensityGrid(const int *density_grid
									,int start_texel
									,int texels
									,float humidity
									,float baseLayer
									,float transition
									,float upperDensity
									,float time
									,void* noise_tex,int octaves,float persistence)

{
simul::base::Timer timer;
timer.StartTime();
std::cout<<"Gpu clouds: FillDensityGrid\n";
	int new_density_gridsize=density_grid[0]*density_grid[1]*density_grid[2];
	dens_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	/*if(!dens_fb.SetTargetFormat(0,iformat,GL_FLOAT))
	{
		itype=GL_INTENSITY;
		if(!dens_fb.SetTargetFormat(0,iformat=GL_INTENSITY32F_ARB,GL_FLOAT))
		{
			itype=GL_RGBA;
			dens_fb.InitColor_Tex(0,iformat=GL_RGBA32F_ARB,GL_FLOAT);
		}
	}*/
	simul::math::Vector3 noise_scale(1.f,1.f,(float)density_grid[2]/(float)density_grid[0]);

	float y_start=(float)start_texel/(float)new_density_gridsize;
	float y_range=(float)(texels)/(float)new_density_gridsize;
	gpuCloudConstants.yRange		=		vec4(y_start,y_range,0,0);

	gpuCloudConstants.octaves		=		octaves;
	gpuCloudConstants.persistence	=		persistence;
	gpuCloudConstants.humidity		=		humidity;
	gpuCloudConstants.time			=		time;
	gpuCloudConstants.noiseScale	=		noise_scale;

	gpuCloudConstants.zPixel		=		1.f/(float)density_grid[2];
	gpuCloudConstants.zSize			=		(float)density_grid[2];
	gpuCloudConstants.baseLayer		=		baseLayer;
	gpuCloudConstants.transition	=		transition;
	gpuCloudConstants.upperDensity	=		upperDensity;

	simul::dx11::setParameter(effect,"volumeNoiseTexture"	,volume_noise_tex_srv);

	gpuCloudConstants.Apply(m_pImmediateContext);
	dens_fb.Activate(m_pImmediateContext);
		ApplyPass(m_pImmediateContext,densityTechnique->GetPassByIndex(0));
		dens_fb.DrawQuad(m_pImmediateContext);
	dens_fb.Deactivate(m_pImmediateContext);
std::cout<<"\tDraw "<<timer.UpdateTime()<<"ms"<<std::endl;
	Ensure3DTextureSizeAndFormat(m_pd3dDevice,density_texture,density_texture_srv,density_grid[0],density_grid[1],density_grid[2],DXGI_FORMAT_R32G32B32A32_FLOAT);
std::cout<<"\tmake 3DTexture "<<timer.UpdateTime()<<"ms"<<std::endl;
	if(start_texel+texels>=new_density_gridsize)
	{
		D3D1x_MAPPED_TEXTURE3D dens_texture_mapped;
		Map3D(m_pImmediateContext,density_texture,&dens_texture_mapped);
		simul::sky::float4 *tex_data=(simul::sky::float4 *)(dens_texture_mapped.pData);
		float *density=new float[4*new_density_gridsize];
		dens_fb.CopyToMemory(m_pImmediateContext,density);
		memcpy(tex_data,density,4*new_density_gridsize*sizeof(float));
		Unmap3D(m_pImmediateContext,density_texture);
		//density_texture	=make3DTexture(m_pd3dDevice,m_pImmediateContext,density_grid[0],density_grid[1],density_grid[2]	,DXGI_FORMAT_R32G32B32A32_FLOAT,density);
		delete [] density;
	}
std::cout<<"\tfill 3DTexture "<<timer.UpdateTime()<<"ms"<<std::endl;

	return NULL;
}

void GpuCloudGenerator::PerformGPURelight(	float *target
											,const int *light_grid
											,int start_texel
											,int texels
											,const int *density_grid
											,const float *Matrix4x4LightToDensityTexcoords
											,const float *lightspace_extinctions_float3)
{
	int gridsize=light_grid[0]*light_grid[1]*light_grid[2];
	
	if(start_texel<0)
		start_texel=0;
	if(start_texel>gridsize)
		start_texel=gridsize;	
	if(start_texel+texels>gridsize)
		texels=gridsize-start_texel; 
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight(light_grid[0],light_grid[1]);
		//fb[i].InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	}
	ID3D1xEffectShaderResourceVariable*	input_light_texture	=effect->GetVariableByName("inputTexture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	densityTexture		=effect->GetVariableByName("densityTexture")->AsShaderResource();

	gpuCloudConstants.yRange			=vec4(0.0,1.0,0,0);
	gpuCloudConstants.transformMatrix	=Matrix4x4LightToDensityTexcoords;
	gpuCloudConstants.transformMatrix.transpose();
	gpuCloudConstants.extinctions		=(lightspace_extinctions_float3);
	// initialize the first input texture.
	simul::dx11::Framebuffer *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	densityTexture->SetResource(density_texture_srv);
	
	if(start_texel==0)
	{
		F[0]->Activate(m_pImmediateContext);
			input_light_texture->SetResource(F[1]->GetBufferResource());
			F[0]->Clear(m_pImmediateContext,1.f,1.f,1.f,1.f,1.f);
		F[0]->Deactivate(m_pImmediateContext);
		F[0]->CopyToMemory(m_pImmediateContext,target);
	}
	int i0	=start_texel/(light_grid[0]*light_grid[1]);
	int i1	=(start_texel+texels)/(light_grid[0]*light_grid[1]);
	target	+=i0*light_grid[0]*light_grid[1]*4;
	if(i0%2)
		std::swap(F[0],F[1]);
	for(int i=i0;i<i1;i++)
	{
		float zPos=((float)i+0.5f)/(float)light_grid[2];
		gpuCloudConstants.zPosition=(zPos);
		gpuCloudConstants.Apply(m_pImmediateContext);
		F[1]->Activate(m_pImmediateContext);
			input_light_texture->SetResource(F[0]->GetBufferResource());
			ApplyPass(m_pImmediateContext,lightingTechnique->GetPassByIndex(0));
			F[1]->DrawQuad(m_pImmediateContext);
		F[1]->Deactivate(m_pImmediateContext);
		// Copy F[1] contents to the target
		F[1]->CopyToMemory(m_pImmediateContext,target);
		std::swap(F[0],F[1]);
		target+=light_grid[0]*light_grid[1]*4;
	}
}

void GpuCloudGenerator::GPUTransferDataToTexture(unsigned char *target
											,const float *DensityToLightTransform
											,const float *light,const int *light_grid
											,const float *ambient,const int *density_grid
											,int start_texel
											,int texels)
{
	// For each level in the z direction, we render out a 2D texture and copy it to the target.
	world_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	world_fb.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
	ID3D11Texture3D* light_texture1		=make3DTexture(m_pd3dDevice,light_grid[0]	,light_grid[1]	,light_grid[2]	,DXGI_FORMAT_R32G32B32A32_FLOAT,light);
	ID3D11ShaderResourceView* light_texture;
	m_pd3dDevice->CreateShaderResourceView(light_texture1,NULL,&light_texture);
	m_pImmediateContext->GenerateMips(light_texture);
	ID3D11Texture3D *ambient_texture1	=make3DTexture(m_pd3dDevice,density_grid[0],density_grid[1],density_grid[2]	,DXGI_FORMAT_R32G32B32A32_FLOAT,ambient);
	ID3D11ShaderResourceView* ambient_texture;
	m_pd3dDevice->CreateShaderResourceView(ambient_texture1,NULL,&ambient_texture);
	m_pImmediateContext->GenerateMips(ambient_texture);

	int new_density_gridsize=density_grid[0]*density_grid[1]*density_grid[2];
	float y_start=(float)start_texel/(float)new_density_gridsize;
	float y_range=(float)(texels)/(float)new_density_gridsize;
	gpuCloudConstants.yRange			=vec4(y_start,y_range,0,0);
	gpuCloudConstants.transformMatrix	=DensityToLightTransform;
	gpuCloudConstants.transformMatrix.transpose();

	gpuCloudConstants.zSize=((float)density_grid[2]);
	gpuCloudConstants.zPixel=(1.f/(float)density_grid[2]);

	setParameter(effect,"densityTexture",density_texture_srv);
	setParameter(effect,"lightTexture",light_texture);
	setParameter(effect,"ambientTexture",ambient_texture);
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.
	gpuCloudConstants.Apply(m_pImmediateContext);
	world_fb.Activate(m_pImmediateContext);
		ApplyPass(m_pImmediateContext,transformTechnique->GetPassByIndex(0));
		world_fb.DrawQuad(m_pImmediateContext);
	world_fb.Deactivate(m_pImmediateContext);
	world_fb.CopyToMemory(m_pImmediateContext,target,start_texel,texels);
	SAFE_RELEASE(ambient_texture);
	SAFE_RELEASE(ambient_texture1);
	SAFE_RELEASE(light_texture);
	SAFE_RELEASE(light_texture1);
}