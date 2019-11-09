#include "SphereRenderer.h"

#if SIMUL_EDITOR
using namespace simul;
using namespace crossplatform;

SphereRenderer::SphereRenderer()
	:renderPlatform(nullptr)
{
}

SphereRenderer::~SphereRenderer()
{
	InvalidateDeviceObjects();
}

void SphereRenderer::RestoreDeviceObjects(RenderPlatform *r)
{
	renderPlatform=r;
	if(!renderPlatform)
		return;
	sphereConstants.RestoreDeviceObjects(r);
	effect.reset(renderPlatform->CreateEffect("sphere"));
}
void SphereRenderer::InvalidateDeviceObjects()
{
	sphereConstants.InvalidateDeviceObjects();
	renderPlatform=nullptr;
	effect.reset();
}
void SphereRenderer::DrawCrossSectionOnSphere(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect, crossplatform::Texture *t, vec2 texcOffset, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour)
{
	math::Matrix4x4 view = deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp, world;
	world.ResetToUnitMatrix();

	world._41 = origin.x;
	world._42 = origin.y;
	world._43 = origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp, world, view, proj);
	sphereConstants.debugWorldViewProj = wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view, (float*)&cam_pos, (float*)&view_dir);
	crossplatform::EffectTechnique*		tech = effect->GetTechniqueByName("draw_cross_section_on_sphere");
	effect->SetTexture(deviceContext, "cloudVolume", t);
	sphereConstants.quaternion = orient_quat;
	sphereConstants.radius = sph_rad;
	sphereConstants.sideview = qsize * 0.5f;
	sphereConstants.debugColour = colour;
	sphereConstants.debugViewDir = view_dir;
	sphereConstants.texcOffset = texcOffset;

	effect->SetConstantBuffer(deviceContext, &sphereConstants);

	deviceContext.renderPlatform->SetTopology(deviceContext, crossplatform::TRIANGLESTRIP);
	effect->Apply(deviceContext, tech, 0);
	deviceContext.renderPlatform->Draw(deviceContext, 16, 0);
	effect->Unapply(deviceContext);
}


void SphereRenderer::DrawLatLongSphere(DeviceContext &deviceContext,int lat, int longt,vec3 origin,float radius,vec4 colour)
{
	//viewport=GetViewport(deviceContext,0);
	math::Matrix4x4 &view=deviceContext.viewStruct.view;
	math::Matrix4x4 &proj = deviceContext.viewStruct.proj;
	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();
	//float tan_x=1.0f/proj(0, 0);
	//float tan_y=1.0f/proj(1, 1);
	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	sphereConstants.multiplier		 = colour;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_lat_long_sphere");
	
	sphereConstants.latitudes		=lat;
	sphereConstants.longitudes		=longt;
	sphereConstants.radius			=radius;
	effect->SetConstantBuffer(deviceContext,&sphereConstants);
	effect->Apply(deviceContext,tech,0);

	renderPlatform->SetTopology(deviceContext,LINESTRIP);
	// first draw the latitudes:
	renderPlatform->Draw(deviceContext, (sphereConstants.longitudes+1)*(sphereConstants.latitudes+1)*2, 0);
	// first draw the longitudes:
	renderPlatform->Draw(deviceContext, (sphereConstants.longitudes+1)*(sphereConstants.latitudes+1)*2, 0);

	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawQuadOnSphere(DeviceContext &deviceContext,vec3 origin,vec4 orient_quat,float qsize,float sph_rad,vec4 colour, vec4 fill_colour)
{
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_quad_on_sphere");

	sphereConstants.quaternion		=orient_quat;
	sphereConstants.radius			=sph_rad;
	sphereConstants.sideview			=qsize*0.5f;
	sphereConstants.debugColour		=colour;
	sphereConstants.multiplier		 = fill_colour;
	sphereConstants.debugViewDir		=view_dir;
	effect->SetConstantBuffer(deviceContext,&sphereConstants);
	if(fill_colour.w>0.0f)
	{
		effect->Apply(deviceContext, tech, "fill");
		renderPlatform->SetTopology(deviceContext, TRIANGLESTRIP);
		renderPlatform->Draw(deviceContext, 4, 0);
		effect->Unapply(deviceContext);
	}
	if (colour.w > 0.0f)
	{
		effect->Apply(deviceContext, tech,"outline");
		renderPlatform->SetTopology(deviceContext, LINELIST);
		renderPlatform->Draw(deviceContext, 16, 0);
		effect->Unapply(deviceContext);
	}
}

void SphereRenderer::DrawTextureOnSphere(DeviceContext &deviceContext,crossplatform::Texture *t,vec3 origin,vec4 orient_quat,float qsize,float sph_rad,vec4 colour)
{
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_texture_on_sphere");
	auto imageTexture=effect->GetShaderResource("imageTexture");
	effect->SetTexture(deviceContext,imageTexture,t);
	sphereConstants.quaternion		=orient_quat;
	sphereConstants.radius			=sph_rad;
	sphereConstants.sideview			=qsize*0.5f;
	sphereConstants.debugColour		=colour;
	sphereConstants.debugViewDir		=view_dir;
	effect->SetConstantBuffer(deviceContext,&sphereConstants);

	renderPlatform->SetTopology(deviceContext,TRIANGLESTRIP);
	effect->Apply(deviceContext,tech,0);
	renderPlatform->Draw(deviceContext,16, 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawCircleOnSphere(DeviceContext &deviceContext, vec3 origin, vec4 orient_quat, float rad,float sph_rad, vec4 colour, vec4 fill_colour)
{
	//Viewport viewport=GetViewport(deviceContext,0);
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_circle_on_sphere");

	sphereConstants.quaternion		=orient_quat;
	sphereConstants.radius			=sph_rad;
	sphereConstants.sideview			=rad;
	sphereConstants.debugColour		=colour; 
	sphereConstants.multiplier		= fill_colour;
	effect->SetConstantBuffer(deviceContext,&sphereConstants);

	if (fill_colour.w > 0.0f)
	{
		effect->Apply(deviceContext, tech, "fill");
		renderPlatform->SetTopology(deviceContext, TRIANGLESTRIP);
		renderPlatform->Draw(deviceContext, 64, 0);
		effect->Unapply(deviceContext);
	}

	effect->Apply(deviceContext,tech,"outline");
	renderPlatform->SetTopology(deviceContext, LINESTRIP);
	renderPlatform->Draw(deviceContext,32, 0);
	effect->Unapply(deviceContext);
}
#endif