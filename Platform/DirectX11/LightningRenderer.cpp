#define NOMINMAX
#include "LightningRenderer.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

LightningRenderer::LightningRenderer(simul::clouds::CloudKeyframer *ck,simul::sky::BaseSkyInterface *sk)
	:BaseLightningRenderer(ck,sk)
	,effect(NULL)
	,inputLayout(NULL)
{
}

LightningRenderer::~LightningRenderer()
{
}

void LightningRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	BaseLightningRenderer::RestoreDeviceObjects(r);
	m_pd3dDevice=renderPlatform->AsD3D11Device();
	vertexBuffer.ensureBufferSize(m_pd3dDevice,1000,NULL,false,true);
	lightningConstants.RestoreDeviceObjects(renderPlatform);
	lightningPerViewConstants.RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
}

void LightningRenderer::RecompileShaders()
{
	SAFE_DELETE(effect);
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]		=ReverseDepth?"1":"0";
	std::vector<crossplatform::EffectDefineOptions> opts;
	opts.push_back(crossplatform::CreateDefineOptions("REVERSE_DEPTH","0","1"));
	renderPlatform->EnsureEffectIsBuilt("lightning",opts);
	effect=renderPlatform->CreateEffect("lightning",defines);
	D3DX11_PASS_DESC PassDesc;
	effect->GetTechniqueByIndex(0)->asD3DX11EffectTechnique()->GetPassByIndex(0)->GetDesc(&PassDesc);
	SAFE_RELEASE(inputLayout);
	const D3D11_INPUT_ELEMENT_DESC mesh_layout_desc[] =
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,	D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,	D3D11_INPUT_PER_VERTEX_DATA,0},
    };
	V_CHECK(m_pd3dDevice->CreateInputLayout(mesh_layout_desc,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&inputLayout));
	lightningConstants			.LinkToEffect(effect,"LightningConstants");
	lightningPerViewConstants	.LinkToEffect(effect,"LightningPerViewConstants");
}

void LightningRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE(effect);
	SAFE_RELEASE(inputLayout);
	lightningConstants.InvalidateDeviceObjects();
	lightningPerViewConstants.InvalidateDeviceObjects();
	vertexBuffer.release();
}

void LightningRenderer::Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depth_tex,simul::sky::float4 depthViewportXYWH,crossplatform::Texture *cloud_depth_tex)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(pContext,"LightningRenderer::Render")
	const simul::clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();
	LightningVertex *vertices=vertexBuffer.Map(pContext);
	if(!vertices)
		return;

	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout(&previousInputLayout);
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);

	D3D11_VIEWPORT viewport;
	UINT num_v		=1;
	pContext->RSGetViewports(&num_v,&viewport);

	math::Matrix4x4 wvp;
	camera::MakeViewProjMatrix(wvp,(const float*)&deviceContext.viewStruct.view,(const float*)&deviceContext.viewStruct.proj);
	lightningPerViewConstants.worldViewProj=wvp;
	lightningPerViewConstants.worldViewProj.transpose();
	
	lightningPerViewConstants.viewportPixels=vec2(viewport.Width,viewport.Height);
	lightningPerViewConstants._line_width	=4;
	lightningPerViewConstants.viewportToTexRegionScaleBias			=vec4(depthViewportXYWH.z,depthViewportXYWH.w,depthViewportXYWH.x,depthViewportXYWH.y);

	lightningPerViewConstants.Apply(deviceContext);
	std::vector<int> start;
	std::vector<int> count;
	std::vector<bool> thick;
	int v=0;
	float time	=baseSkyInterface->GetTime();
	for(int i=0;i<cloudKeyframer->GetNumLightningBolts(time);i++)
	{
		const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,i);
		simul::clouds::LightningProperties props	=cloudKeyframer->GetLightningProperties(time,i);
		if(!lightningRenderInterface)
			continue;
		if(!props.numLevels)
			continue;
		lightningConstants.lightningColour	=props.colour;
		lightningConstants.Apply(deviceContext);

		//dx11::setParameter(effect,"lightningColour",props.colour);
		simul::sky::float4 x1,x2;
		static float maxwidth=8.f;
		static float pixel_width_threshold=3.f;
		simul::math::Vector3 cam_pos=GetCameraPosVector((const float*)&deviceContext.viewStruct.view);
		float vertical_shift=0;//helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(int j=0;j<props.numLevels;j++)
		{
			for(int jj=0;jj<(j>0?props.branchCount:1);jj++)
			{
				const simul::clouds::LightningRenderInterface::Branch &branch=lightningRenderInterface->GetBranch(props,time,j,jj);
				float dist=0.001f*(cam_pos-simul::math::Vector3(branch.vertices[0])).Magnitude();

				int v_start=v;
				start.push_back(v);
				math::Vector3 diff=((const float *)branch.vertices[0]);
				diff-=cam_pos;
				float pixel_width=branch.width/diff.Magnitude()*viewport.Width;
				bool draw_thick=pixel_width>pixel_width_threshold;
				thick.push_back(draw_thick);

				for(int k=draw_thick?-1:0;k<branch.numVertices;k++)
				{
					if(v>=1000)
						break;
					bool start=(k<0);
					if(start)
					{
						x1=(const float *)branch.vertices[k+2];
						if(k+1<branch.numVertices)
						{
							x2=(const float *)branch.vertices[k+1];
							simul::sky::float4 dir=x2-x1;
							x1=x2+dir;
						}
					}
					else
						x1=(const float *)branch.vertices[k];
					bool end=(k==branch.numVertices-1);

					float brightness=branch.brightness;
					float dh=x1.z/1000.f-K.cloud_base_km;
					if(dh>0)
					{
						static float cc=0.1f;
						float d=exp(-dh/cc);
						brightness*=d;
					}
					if(end)
						brightness=0.f;
					if(!draw_thick)
						brightness*=pixel_width/pixel_width_threshold;
					vertices[v].texCoords=vec4(branch.width*x1.w,branch.width*x1.w,x1.w,brightness*x1.w);
					vertices[v].position=vec4(x1.x,x1.y,x1.z+vertical_shift,x1.w);
					v++;
				}
				count.push_back(v-v_start);
			}
		}
	}
	vertexBuffer.Unmap(pContext);
	vertexBuffer.apply(pContext,0);
	pContext->IASetInputLayout(inputLayout);

	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("lightning_thick");
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ);
	if(depth_tex->GetSampleCount()>0)
		effect->SetTexture(deviceContext,"depthTextureMS",depth_tex);
	else
		effect->SetTexture(deviceContext,"depthTexture",depth_tex);
	effect->SetTexture(deviceContext,"cloudDepthTexture",cloud_depth_tex);
	effect->Apply(deviceContext,tech,0);
	for(int i=0;i<(int)start.size();i++)
	{
		if(!thick[i])
		{
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
			tech=effect->GetTechniqueByName("lightning_thin");
			effect->Unapply(deviceContext);
			effect->Apply(deviceContext,tech,0);
		}
		if(count[i]>0)
			pContext->Draw(count[i],start[i]);
	}
	effect->Unapply(deviceContext);
	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout(previousInputLayout);
	SIMUL_COMBINED_PROFILE_END(pContext)
}