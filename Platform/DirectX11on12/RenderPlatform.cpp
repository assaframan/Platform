#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX11on12/RenderPlatform.h"
#include "Simul/Platform/DirectX11on12/Material.h"
#include "Simul/Platform/DirectX11on12/Mesh.h"
#include "Simul/Platform/DirectX11on12/Texture.h"
#include "Simul/Platform/DirectX11on12/ESRAMTexture.h"
#include "Simul/Platform/DirectX11on12/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11on12/Light.h"
#include "Simul/Platform/DirectX11on12/Effect.h"
#include "Simul/Platform/DirectX11on12/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11on12/Buffer.h"
#include "Simul/Platform/DirectX11on12/Layout.h"
#include "Simul/Platform/DirectX11on12/MacrosDX1x.h"
#include "Simul/Platform/DirectX11on12/SaveTextureDX1x.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11on12/CompileShaderDX1x.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "D3dx11effect.h"
#ifdef _XBOX_ONE
#include "Simul/Platform/DirectX11/ESRAMManager.h"
#include <D3Dcompiler_x.h>
#else
#include <D3Dcompiler.h>
#endif
#include <algorithm>
#ifdef SIMUL_ENABLE_PIX
#include "pix.h"
#endif

#include "Simul/Platform/DirectX11on12/Utilities.h"

using namespace simul;
using namespace dx11on12;

struct Vertex3_t
{
	float x,y,z;
};

static const float size=5.f;
static Vertex3_t box_vertices[36] =
{
	{-size,		-size,	size},
	{size,		size,	size},
	{size,		-size,	size},
	{size,		size,	size},
	{-size,		-size,	size},
	{-size,		size,	size},
	
	{-size,		-size,	-size},
	{ size,		-size,	-size},
	{ size,		 size,	-size},
	{ size,		 size,	-size},
	{-size,		 size,	-size},
	{-size,		-size,	-size},
	
	{-size,		size,	-size},
	{ size,		size,	-size},
	{ size,		size,	 size},
	{ size,		size,	 size},
	{-size,		size,	 size},
	{-size,		size,	-size},
				
	{-size,		-size,  -size},
	{ size,		-size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
	{-size,		-size,  -size},
	{-size,		-size,	 size},
	
	{ size,		-size,	-size},
	{ size,		 size,	 size},
	{ size,		 size,	-size},
	{ size,		 size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
				
	{-size,		-size,	-size},
	{-size,		 size,	-size},
	{-size,		 size,	 size},
	{-size,		 size,	 size},
	{-size,		-size,	 size},
	{-size,		-size,	-size},
};

void RenderPlatform::StoredState::Clear()
{
	m_StencilRefStored11			=0;
	m_SampleMaskStored11			=0;
	m_indexOffset					=0;
	m_indexFormatStored11			=DXGI_FORMAT_UNKNOWN;
	m_previousTopology				=D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	m_pDepthStencilStateStored11	=NULL;
	m_pRasterizerStateStored11		=NULL;
	m_pBlendStateStored11			=NULL;
	pIndexBufferStored11			=NULL;
	m_previousInputLayout			=NULL;
	pVertexShader					=NULL;
	pPixelShader					=NULL;
	pHullShader						=NULL;
	pDomainShader					=NULL;
	pGeometryShader					=NULL;
	pComputeShader					=NULL;

	for(int i=0;i<4;i++)
		m_BlendFactorStored11[i];
	for(int i=0;i<D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;i++)
	{
		m_pSamplerStateStored11[i]=NULL;
		m_pVertexSamplerStateStored11[i]=NULL;
		m_pComputeSamplerStateStored11[i]=NULL;
		m_pGeometrySamplerStateStored11[i]=NULL;
	}
	for(int i=0;i<D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;i++)
	{
		m_pCSConstantBuffers[i]=NULL;
		m_pGSConstantBuffers[i]=NULL;
		m_pPSConstantBuffers[i]=NULL;
		m_pVSConstantBuffers[i]=NULL;
		m_pHSConstantBuffers[i]=NULL;
		m_pDSConstantBuffers[i]=NULL;
	}
	for (int i = 0; i<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
	{
		m_pShaderResourceViews[i]=NULL;
		m_pCSShaderResourceViews[i]=NULL;
	}
	for (int i = 0; i<D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		m_pUnorderedAccessViews[i] = NULL;
	}
	for(int i=0;i<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;i++)
	{
		m_pVertexBuffersStored11[i]=NULL;
		m_VertexStrides[i]=0;
		m_VertexOffsets[i]=0;
	}
	numPixelClassInstances=0;
	for(int i=0;i<16;i++)
	{
		m_pPixelClassInstances[i]=NULL;
	}
	numVertexClassInstances=0;
	for(int i=0;i<16;i++)
	{
		m_pVertexClassInstances[i]=NULL;
	}
}

RenderPlatform::RenderPlatform()
	:device(NULL)
	,m_pCubemapVtxDecl(NULL)
	,m_pVertexBuffer(NULL)
	,m_pVtxDecl(NULL)
	,storedStateCursor(0)
	,pUserDefinedAnnotation(NULL)
{
	storedStates.resize(4);
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

#ifdef _XBOX_ONE
ESRAMManager *eSRAMManager=NULL;
#endif
void RenderPlatform::RestoreDeviceObjects(void *d)
{
	if(device==d)
		return;
	device=(ID3D11Device*)d;
	crossplatform::RenderPlatform::RestoreDeviceObjects(d);
	
	ID3D11DeviceContext *pImmediateContext;
	AsD3D11Device()->GetImmediateContext(&pImmediateContext);
	immediateContext.platform_context=pImmediateContext;
	immediateContext.renderPlatform=this;
#ifdef _XBOX_ONE
	delete eSRAMManager;
	eSRAMManager=new ESRAMManager(device);
#endif
	RecompileShaders();
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pCubemapVtxDecl);
	// Vertex declaration
	if(debugEffect)
	{
		D3DX11_PASS_DESC PassDesc;
		crossplatform::EffectTechnique *tech	=debugEffect->GetTechniqueByName("vec3_input_signature");
		if(tech&&tech->asD3DX11EffectTechnique())
		{
			tech->asD3DX11EffectTechnique()->GetPassByIndex(0)->GetDesc(&PassDesc);
			D3D11_INPUT_ELEMENT_DESC decl[]=
			{
				{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			V_CHECK(AsD3D11Device()->CreateInputLayout(decl,1,PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pCubemapVtxDecl));
		}
	}
	D3D11_BUFFER_DESC desc=
	{
        36*sizeof(vec3),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0,
        0
	};
	
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
    InitData.pSysMem		=box_vertices;
    InitData.SysMemPitch	=sizeof(vec3);
	V_CHECK(AsD3D11Device()->CreateBuffer(&desc,&InitData,&m_pVertexBuffer));

	RecompileShaders();
	
#ifdef SIMUL_WIN8_SDK
	{
		SAFE_RELEASE(pUserDefinedAnnotation);
		IUnknown *unknown=(IUnknown *)pImmediateContext;
#ifdef _XBOX_ONE
		V_CHECK(unknown->QueryInterface( __uuidof(pUserDefinedAnnotation), reinterpret_cast<void**>(&pUserDefinedAnnotation) ));
#else
		V_CHECK(unknown->QueryInterface(IID_PPV_ARGS(&pUserDefinedAnnotation)));
#endif
}
#endif
}

void RenderPlatform::InvalidateDeviceObjects()
{
#ifdef _XBOX_ONE
	delete eSRAMManager;
#endif
#ifdef SIMUL_WIN8_SDK
	SAFE_RELEASE(pUserDefinedAnnotation);
#endif
	SAFE_RELEASE(m_pVtxDecl);
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
	for(std::set<crossplatform::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx11on12::Material *mat=(dx11on12::Material*)(*i);
		delete mat;
	}
	materials.clear();
	SAFE_RELEASE(m_pCubemapVtxDecl);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_DELETE(debugEffect);
	ID3D11DeviceContext* c=immediateContext.asD3D11DeviceContext();
	SAFE_RELEASE(c);
	immediateContext.platform_context=NULL;
	device=NULL;
}

void RenderPlatform::RecompileShaders()
{
	if(!device)
		return;
	crossplatform::RenderPlatform::RecompileShaders();
	for(std::set<crossplatform::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx11on12::Material *mat=(dx11on12::Material*)(*i);
		mat->SetEffect(solidEffect);
	}
}

void RenderPlatform::BeginEvent			(crossplatform::DeviceContext &deviceContext,const char *name)
{
#ifdef SIMUL_WIN8_SDK
		if(pUserDefinedAnnotation)
			pUserDefinedAnnotation->BeginEvent(base::StringToWString(name).c_str());
#endif
#ifdef SIMUL_ENABLE_PIX
	if(last_name==string(name))
		PIXBeginEvent( 0, name, name );
#endif
}

void RenderPlatform::EndEvent			(crossplatform::DeviceContext &deviceContext)
{
#ifdef SIMUL_WIN8_SDK
	if(pUserDefinedAnnotation)
		pUserDefinedAnnotation->EndEvent();
#endif
#ifdef SIMUL_ENABLE_PIX
	PIXEndEvent();
#endif
}

void RenderPlatform::StartRender(crossplatform::DeviceContext &deviceContext)
{
	/*
	
	glEnable(GL_DEPTH_TEST);
	// Draw the front face only, except for the texts and lights.
	glEnable(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);
	glUseProgram(solid_program);*/
	simul::crossplatform::Frustum frustum = simul::crossplatform::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	SetStandardRenderState(deviceContext, frustum.reverseDepth ? crossplatform::STANDARD_TEST_DEPTH_GREATER_EQUAL : crossplatform::STANDARD_TEST_DEPTH_LESS_EQUAL);
}

void RenderPlatform::EndRender(crossplatform::DeviceContext &)
{
	/*glUseProgram(0);
	
	*/
}

namespace
{
    const float DEFAULT_LIGHT_POSITION[]				={0.0f, 0.0f, 0.0f, 1.0f};
    const float DEFAULT_DIRECTION_LIGHT_POSITION[]	={0.0f, 0.0f, 1.0f, 0.0f};
    const float DEFAULT_SPOT_LIGHT_DIRECTION[]		={0.0f, 0.0f, -1.0f};
    const float DEFAULT_LIGHT_COLOR[]					={1.0f, 1.0f, 1.0f, 1.0f};
    const float DEFAULT_LIGHT_SPOT_CUTOFF				=180.0f;
}

void RenderPlatform::IntializeLightingEnvironment(const float pAmbientLight[3])
{
 /*   glLightfv(GL_LIGHT0, GL_POSITION, DEFAULT_DIRECTION_LIGHT_POSITION);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, DEFAULT_LIGHT_COLOR);
    glLightfv(GL_LIGHT0, GL_SPECULAR, DEFAULT_LIGHT_COLOR);
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, DEFAULT_LIGHT_SPOT_CUTOFF);
    glEnable(GL_LIGHT0);
    // Set ambient light.
    GLfloat lAmbientLight[] = {static_cast<GLfloat>(pAmbientLight[0]), static_cast<GLfloat>(pAmbientLight[1]),static_cast<GLfloat>(pAmbientLight[2]), 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lAmbientLight);*/
}

void RenderPlatform::CopyTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *t,crossplatform::Texture *s)
{
	if(!t||!s)
	{
		SIMUL_BREAK_ONCE("Can't copy null texture");
		return;
	}
	ID3D11Resource *T=t->AsD3D11Resource();
	ID3D11Resource *S=s->AsD3D11Resource();
	if(T!=nullptr&&S!=nullptr)
	{
		deviceContext.asD3D11DeviceContext()->CopyResource(T,S);
	}
	else
	{
		return;
	}
}

D3D11_MAP_FLAG RenderPlatform::GetMapFlags()
{
#ifdef _XBOX_ONE
	if(UsesFastSemantics())
		return D3D11_MAP_FLAG_ALLOW_USAGE_DEFAULT;
	else
#endif
		return (D3D11_MAP_FLAG)0;
}
void RenderPlatform::DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	ApplyContextState(deviceContext);
	deviceContext.asD3D11DeviceContext()->Dispatch(w,l,d);
}

void RenderPlatform::ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,crossplatform::EffectTechnique *tech,int index)
{
	tech->asD3DX11EffectTechnique()->GetPassByIndex(index)->Apply(0,deviceContext.asD3D11DeviceContext());
}

void RenderPlatform::Draw			(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert)
{
	if(!ApplyContextState(deviceContext))
		return;
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	pContext->Draw(num_verts,start_vert);
}

void RenderPlatform::DrawIndexed(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index,int base_vert)
{
	ApplyContextState(deviceContext);
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	pContext->DrawIndexed(num_indices,start_index,base_vert);
}

void RenderPlatform::DrawMarker(crossplatform::DeviceContext &deviceContext,const double *matrix)
{
 /*   glColor3f(0.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	
    
    glMultMatrixd((const double*) matrix);

    glBegin(GL_LINE_LOOP);
        glVertex3f(+1.0f, -1.0f, +1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, +1.0f);

        glVertex3f(+1.0f, +1.0f, +1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);

        glVertex3f(+1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(+1.0f, -1.0f, +1.0f);

        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);

        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, +1.0f);

        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
    glEnd();
    */
}


void RenderPlatform::DrawCrossHair(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition)
{
/*    glColor3f(1.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	
    
    glMultMatrixd((double*) pGlobalPosition);

    double lCrossHair[6][3] = { { -3, 0, 0 }, { 3, 0, 0 },
    { 0, -3, 0 }, { 0, 3, 0 },
    { 0, 0, -3 }, { 0, 0, 3 } };

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[0]);
    glVertex3dv(lCrossHair[1]);

    glEnd();

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[2]);
    glVertex3dv(lCrossHair[3]);

    glEnd();

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[4]);
    glVertex3dv(lCrossHair[5]);

    glEnd();
	
	
    */
}

void RenderPlatform::DrawCamera(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll)
{
 /*   glColor3d(1.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	
    
    glMultMatrixd((const double*) pGlobalPosition);
    glRotated(pRoll, 1.0, 0.0, 0.0);

    int i;
    float lCamera[10][2] = {{ 0, 5.5 }, { -3, 4.5 },
    { -3, 7.5 }, { -6, 10.5 }, { -23, 10.5 },
    { -23, -4.5 }, { -20, -7.5 }, { -3, -7.5 },
    { -3, -4.5 }, { 0, -5.5 }   };

    glBegin( GL_LINE_LOOP );
    {
        for (i = 0; i < 10; i++)
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], 4.5);
        }
    }
    glEnd();

    glBegin( GL_LINE_LOOP );
    {
        for (i = 0; i < 10; i++)
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], -4.5);
        }
    }
    glEnd();

    for (i = 0; i < 10; i++)
    {
        glBegin( GL_LINES );
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], -4.5);
            glVertex3f(lCamera[i][0], lCamera[i][1], 4.5);
        }
        glEnd();
    }
    */
}

void RenderPlatform::DrawLineLoop(crossplatform::DeviceContext &deviceContext,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
{
/*    
    glMultMatrixd((const double*)mat);
	glColor3f(colr[0],colr[1],colr[2]);
	glBegin(GL_LINE_LOOP);
	for (int lVerticeIndex = 0; lVerticeIndex < lVerticeCount; lVerticeIndex++)
	{
		glVertex3dv((GLdouble *)&vertexArray[lVerticeIndex*3]);
	}
	glEnd();
    */
}

void RenderPlatform::ApplyDefaultMaterial()
{
    const float BLACK_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float GREEN_COLOR[] = {0.0f, 1.0f, 0.0f, 1.0f};
//    const GLfloat WHITE_COLOR[] = {1.0f, 1.0f, 1.0f, 1.0f};
/*    glMaterialfv(GL_FRONT, GL_EMISSION, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_AMBIENT, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, GREEN_COLOR);
    glMaterialfv(GL_FRONT, GL_SPECULAR, BLACK_COLOR);
    glMaterialf(GL_FRONT, GL_SHININESS, 0);

    glBindTexture(GL_TEXTURE_2D, 0);*/
}


crossplatform::Material *RenderPlatform::CreateMaterial()
{
	dx11on12::Material *mat=new dx11on12::Material;
	mat->SetEffect(solidEffect);
	materials.insert(mat);
	return mat;
}

crossplatform::Mesh *RenderPlatform::CreateMesh()
{
	return new dx11on12::Mesh;
}

crossplatform::Light *RenderPlatform::CreateLight()
{
	return new dx11on12::Light();
}

crossplatform::Texture *RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	ERRNO_BREAK
	crossplatform::Texture * tex=NULL;
#ifdef _XBOX_ONE
	//if(fileNameUtf8&&strcmp(fileNameUtf8,"ESRAM")==0)
	if(eSRAMManager&&eSRAMManager->IsEnabled())
		tex=new dx11::ESRAMTexture();
	else
#endif
	tex=new dx11on12::Texture();
	if(fileNameUtf8&&strlen(fileNameUtf8)>0)
	{
		if(strstr( fileNameUtf8,".")!=nullptr)
			tex->LoadFromFile(this,fileNameUtf8);
		tex->SetName(fileNameUtf8);
	}
	
	ERRNO_BREAK
	return tex;
}

crossplatform::BaseFramebuffer *RenderPlatform::CreateFramebuffer(const char *name)
{
	return new dx11on12::Framebuffer(name);
}

static D3D11_TEXTURE_ADDRESS_MODE toD3d11TextureAddressMode(crossplatform::SamplerStateDesc::Wrapping f)
{
	if(f==crossplatform::SamplerStateDesc::CLAMP)
		return D3D11_TEXTURE_ADDRESS_CLAMP;
	if(f==crossplatform::SamplerStateDesc::WRAP)
		return D3D11_TEXTURE_ADDRESS_WRAP;
	if(f==crossplatform::SamplerStateDesc::MIRROR)
		return D3D11_TEXTURE_ADDRESS_MIRROR;
	return D3D11_TEXTURE_ADDRESS_WRAP;
}
D3D11_FILTER toD3d11Filter(crossplatform::SamplerStateDesc::Filtering f)
{
	if(f==crossplatform::SamplerStateDesc::LINEAR)
		return D3D11_FILTER_ANISOTROPIC;
	return D3D11_FILTER_MIN_MAG_MIP_POINT;
}
crossplatform::SamplerState *RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc *d)
{
	dx11on12::SamplerState *s=new dx11on12::SamplerState(d);
	D3D11_SAMPLER_DESC samplerDesc;
	
    ZeroMemory( &samplerDesc, sizeof( D3D11_SAMPLER_DESC ) );
    samplerDesc.Filter = toD3d11Filter (d->filtering) ;
    samplerDesc.AddressU = toD3d11TextureAddressMode(d->x);
    samplerDesc.AddressV = toD3d11TextureAddressMode(d->y);
    samplerDesc.AddressW = toD3d11TextureAddressMode(d->z);
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxAnisotropy = 16;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	
	SAFE_RELEASE(s->m_pd3D11SamplerState);
	AsD3D11Device()->CreateSamplerState(&samplerDesc,&s->m_pd3D11SamplerState);
	s->default_slot=d->slot;
	return s;
}

crossplatform::Effect *RenderPlatform::CreateEffect()
{
	crossplatform::Effect *e= new dx11on12::Effect();
	return e;
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	crossplatform::PlatformConstantBuffer *b=new dx11on12::PlatformConstantBuffer();
	return b;
}

crossplatform::PlatformStructuredBuffer *RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer *b=new dx11on12::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer *RenderPlatform::CreateBuffer()
{
	crossplatform::Buffer *b=new dx11on12::Buffer();
	return b;
}

DXGI_FORMAT RenderPlatform::ToDxgiFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case R_16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case RGBA_16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case RGB_11_11_10_FLOAT:
		return DXGI_FORMAT_R11G11B10_FLOAT;
	case RGBA_32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case RGB_32_FLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case RG_16_FLOAT:
		return DXGI_FORMAT_R16G16_FLOAT;
	case RG_32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case R_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case LUM_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case INT_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case RGBA_8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case RGBA_8_UNORM_COMPRESSED:
		return DXGI_FORMAT_BC7_UNORM;
	case RGBA_8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case R_8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case R_8_SNORM:
		return DXGI_FORMAT_R8_SNORM;
	case R_32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case RG_32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
	case RGB_32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case RGBA_32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	case D_32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case D_16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case D_24_UNORM_S_8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	};
}

crossplatform::PixelFormat RenderPlatform::FromDxgiFormat(DXGI_FORMAT f)
{
	using namespace crossplatform;
	switch(f)
	{
	case DXGI_FORMAT_R16_FLOAT:
		return R_16_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return RGBA_16_FLOAT;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return RGBA_32_FLOAT;
	case DXGI_FORMAT_R32G32B32_FLOAT:
		return RGB_32_FLOAT;
	case DXGI_FORMAT_R32G32_FLOAT:
		return RG_32_FLOAT;
	case DXGI_FORMAT_R32_FLOAT:
		return R_32_FLOAT;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return RGBA_8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_SNORM:
		return RGBA_8_SNORM;
	case DXGI_FORMAT_B8G8R8A8_UNORM:		// What possible reason is there for this to exist?
		return RGBA_8_UNORM;
	case DXGI_FORMAT_R32_UINT:
		return R_32_UINT;
	case DXGI_FORMAT_R32G32_UINT:
		return RG_32_UINT;
	case DXGI_FORMAT_R32G32B32_UINT:
		return RGB_32_UINT;
	case DXGI_FORMAT_R32G32B32A32_UINT:
		return RGBA_32_UINT;
	case DXGI_FORMAT_D32_FLOAT:
		return D_32_FLOAT;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return D_24_UNORM_S_8_UINT;
	case DXGI_FORMAT_D16_UNORM:
		return D_16_UNORM;
	default:
		return UNKNOWN;
	};
}

crossplatform::ShaderResourceType RenderPlatform::FromD3DShaderVariableType(D3D_SHADER_VARIABLE_TYPE t)
{
	using namespace crossplatform;
	switch(t)
	{
	case D3D_SVT_TEXTURE1D:
		return ShaderResourceType::TEXTURE_1D;
	case D3D_SVT_TEXTURE2D:
		return ShaderResourceType::TEXTURE_2D;
	case D3D_SVT_TEXTURE3D:
		return ShaderResourceType::TEXTURE_3D;
	case D3D_SVT_TEXTURECUBE:
		return ShaderResourceType::TEXTURE_CUBE;
	case D3D_SVT_SAMPLER:
		return ShaderResourceType::SAMPLER;
	case D3D_SVT_BUFFER:
		return ShaderResourceType::BUFFER;
	case D3D_SVT_CBUFFER:
		return ShaderResourceType::CBUFFER;
	case D3D_SVT_TBUFFER:
		return ShaderResourceType::TBUFFER;
	case D3D_SVT_TEXTURE1DARRAY:
		return ShaderResourceType::TEXTURE_1D_ARRAY;
	case D3D_SVT_TEXTURE2DARRAY:
		return ShaderResourceType::TEXTURE_2D_ARRAY;
	case D3D_SVT_TEXTURE2DMS:
		return ShaderResourceType::TEXTURE_2DMS;
	case D3D_SVT_TEXTURE2DMSARRAY:
		return ShaderResourceType::TEXTURE_2DMS_ARRAY;
	case D3D_SVT_TEXTURECUBEARRAY:
		return ShaderResourceType::TEXTURE_CUBE_ARRAY;
	case D3D_SVT_RWTEXTURE1D:
		return ShaderResourceType::RW_TEXTURE_1D;
	case D3D_SVT_RWTEXTURE1DARRAY:
		return ShaderResourceType::RW_TEXTURE_1D_ARRAY;
	case D3D_SVT_RWTEXTURE2D:
		return ShaderResourceType::RW_TEXTURE_2D;
	case D3D_SVT_RWTEXTURE2DARRAY:
		return ShaderResourceType::RW_TEXTURE_2D_ARRAY;
	case D3D_SVT_RWTEXTURE3D:
		return ShaderResourceType::RW_TEXTURE_3D;
	case D3D_SVT_RWBUFFER:
		return ShaderResourceType::RW_BUFFER;
	case D3D_SVT_BYTEADDRESS_BUFFER:
		return ShaderResourceType::BYTE_ADDRESS_BUFFER;
	case D3D_SVT_RWBYTEADDRESS_BUFFER:
		return ShaderResourceType::RW_BYTE_ADDRESS_BUFFER;
	case D3D_SVT_STRUCTURED_BUFFER:
		return ShaderResourceType::STRUCTURED_BUFFER;
	case D3D_SVT_RWSTRUCTURED_BUFFER:
		return ShaderResourceType::RW_STRUCTURED_BUFFER;
	case D3D_SVT_APPEND_STRUCTURED_BUFFER:
		return ShaderResourceType::APPEND_STRUCTURED_BUFFER;
	case D3D_SVT_CONSUME_STRUCTURED_BUFFER:
		return ShaderResourceType::CONSUME_STRUCTURED_BUFFER;
	default:
		return ShaderResourceType::COUNT;
	}
}

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,const crossplatform::LayoutDesc *desc)
{
	D3D11_INPUT_ELEMENT_DESC *decl=new D3D11_INPUT_ELEMENT_DESC[num_elements];
	for(int i=0;i<num_elements;i++)
	{
		const crossplatform::LayoutDesc &d=desc[i];
		D3D11_INPUT_ELEMENT_DESC &D=decl[i];
		D.SemanticName=d.semanticName;
		D.SemanticIndex=d.semanticIndex;
		D.Format=ToDxgiFormat(d.format);
		D.AlignedByteOffset=d.alignedByteOffset;
		D.InputSlot=d.inputSlot;
		D.InputSlotClass=d.perInstance?D3D11_INPUT_PER_INSTANCE_DATA:D3D11_INPUT_PER_VERTEX_DATA;
		D.InstanceDataStepRate=d.instanceDataStepRate;
	};
	dx11on12::Layout *l=new dx11on12::Layout();
	l->SetDesc(desc,num_elements);
	ID3DBlob *VS;
	ID3DBlob *errorMsgs=NULL;
	std::string dummy_shader;
	dummy_shader="struct vertexInput\n"
				"{\n";
	for(int i=0;i<num_elements;i++)
	{
		D3D11_INPUT_ELEMENT_DESC &dec=decl[i];
		std::string format;
		switch(dec.Format)
		{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			format="float4";
			break;
		case DXGI_FORMAT_R32G32B32_FLOAT:
			format="float3";
			break;
		case DXGI_FORMAT_R32G32_FLOAT:
			format="float2";
			break;
		case DXGI_FORMAT_R32_FLOAT:
			format="float";
			break;
		case DXGI_FORMAT_R32_UINT:
			format="uint";
			break;
		default:
			SIMUL_CERR<<"Unhandled type "<<std::endl;
		};
		dummy_shader+="   ";
		dummy_shader+=format+" ";
		std::string name=dec.SemanticName;
		if(strcmp(dec.SemanticName,"POSITION")!=0)
			name+=('0'+dec.SemanticIndex);
		dummy_shader+=name;
		dummy_shader+="_";
		dummy_shader+=" : ";
		dummy_shader+=name;
		dummy_shader+=";\n";
				//"	float3 position		: POSITION;"
				//"	float texCoords		: TEXCOORD0;";
	}
	dummy_shader+="};\n"
				"struct vertexOutput\n"
				"{\n"
				"	float4 hPosition	: SV_POSITION;\n"
				"};\n"
				"vertexOutput VS_Main(vertexInput IN)\n"
				"{\n"
				"	vertexOutput OUT;\n"
				"	OUT.hPosition	=float4(1.0,1.0,1.0,1.0);\n"
				"	return OUT;\n"
				"}\n";
	const char *str=dummy_shader.c_str();
	size_t len=strlen(str);
#if WINVER<0x602
	HRESULT hr=D3DX11CompileFromMemory(str,len,"dummy",NULL,NULL,"VS_Main", "vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL0, 0, 0, &VS, &errorMsgs, 0);
#else
	HRESULT hr=D3DCompile(str,len,"dummy",NULL,NULL,"VS_Main", "vs_4_0", 0, 0, &VS, &errorMsgs);
#endif
	if(hr!=S_OK)
	{
		const char *e=(const char*)errorMsgs->GetBufferPointer();
		std::cerr<<e<<std::endl;
	}
	SAFE_RELEASE(l->d3d11InputLayout);
	AsD3D11Device()->CreateInputLayout(decl, num_elements, VS->GetBufferPointer(), VS->GetBufferSize(), &l->d3d11InputLayout);
	
	if(VS)
		VS->Release();
	if(errorMsgs)
		errorMsgs->Release();
	delete [] decl;
	return l;
}
static D3D11_COMPARISON_FUNC toD3dComparison(crossplatform::DepthComparison d)
{
	switch(d)
	{
	case crossplatform::DEPTH_LESS:
		return D3D11_COMPARISON_LESS;
	case crossplatform::DEPTH_EQUAL:
		return D3D11_COMPARISON_EQUAL;
	case crossplatform::DEPTH_LESS_EQUAL:
		return D3D11_COMPARISON_LESS_EQUAL;
	case crossplatform::DEPTH_GREATER:
		return D3D11_COMPARISON_GREATER;
	case crossplatform::DEPTH_NOT_EQUAL:
		return D3D11_COMPARISON_NOT_EQUAL;
	case crossplatform::DEPTH_GREATER_EQUAL:
		return D3D11_COMPARISON_GREATER_EQUAL;
	default:
		break;
	};
	return D3D11_COMPARISON_LESS;
}
static D3D11_BLEND_OP toD3dBlendOp(crossplatform::BlendOperation o)
{
	switch(o)
	{
	case crossplatform::BLEND_OP_ADD:
		return D3D11_BLEND_OP_ADD;
	case crossplatform::BLEND_OP_SUBTRACT:
		return D3D11_BLEND_OP_SUBTRACT;
	case crossplatform::BLEND_OP_MAX:
		return D3D11_BLEND_OP_MAX;
	case crossplatform::BLEND_OP_MIN:
		return D3D11_BLEND_OP_MIN;
	default:
		break;
	};
	return D3D11_BLEND_OP_ADD;
}
static D3D11_BLEND toD3dBlend(crossplatform::BlendOption o)
{
	switch(o)
	{
	case crossplatform::BLEND_ZERO:
		return D3D11_BLEND_ZERO;
	case crossplatform::BLEND_ONE:
		return D3D11_BLEND_ONE;
	case crossplatform::BLEND_SRC_COLOR:
		return D3D11_BLEND_SRC_COLOR;
	case crossplatform::BLEND_INV_SRC_COLOR:
		return D3D11_BLEND_INV_SRC_COLOR;
	case crossplatform::BLEND_SRC_ALPHA:
		return D3D11_BLEND_SRC_ALPHA;
	case crossplatform::BLEND_INV_SRC_ALPHA:
		return D3D11_BLEND_INV_SRC_ALPHA;
	case crossplatform::BLEND_DEST_ALPHA:
		return D3D11_BLEND_DEST_ALPHA;
	case crossplatform::BLEND_INV_DEST_ALPHA:
		return D3D11_BLEND_INV_DEST_ALPHA;
	case crossplatform::BLEND_DEST_COLOR:
		return D3D11_BLEND_DEST_COLOR;
	case crossplatform::BLEND_INV_DEST_COLOR:
		return D3D11_BLEND_INV_DEST_COLOR;
	case crossplatform::BLEND_SRC_ALPHA_SAT:
		return D3D11_BLEND_SRC_ALPHA_SAT;
	case crossplatform::BLEND_BLEND_FACTOR:
		return D3D11_BLEND_BLEND_FACTOR;
	case crossplatform::BLEND_INV_BLEND_FACTOR:
		return D3D11_BLEND_INV_BLEND_FACTOR;
	case crossplatform::BLEND_SRC1_COLOR:
		return D3D11_BLEND_SRC1_COLOR;
	case crossplatform::BLEND_INV_SRC1_COLOR:
		return D3D11_BLEND_INV_SRC1_COLOR;
	case crossplatform::BLEND_SRC1_ALPHA:
		return D3D11_BLEND_SRC1_ALPHA;
	case crossplatform::BLEND_INV_SRC1_ALPHA:
		return D3D11_BLEND_INV_SRC1_ALPHA;
	default:
		break;
	};
	return D3D11_BLEND_ONE;
}

D3D11_FILL_MODE toD3dFillMode(crossplatform::PolygonMode p)
{
	switch (p)
	{
	case crossplatform::POLYGON_MODE_FILL:
		return D3D11_FILL_MODE::D3D11_FILL_SOLID;
	case crossplatform::POLYGON_MODE_LINE:
	default:
		return D3D11_FILL_MODE::D3D11_FILL_WIREFRAME;
		break;
	};
}

D3D11_CULL_MODE toD3dCullMode(crossplatform::CullFaceMode c)
{
	switch (c)
	{
	case crossplatform::CullFaceMode::CULL_FACE_BACK:
		return D3D11_CULL_MODE::D3D11_CULL_BACK;
	case crossplatform::CullFaceMode::CULL_FACE_FRONT:
		return D3D11_CULL_MODE::D3D11_CULL_FRONT;
	case crossplatform::CullFaceMode::CULL_FACE_FRONTANDBACK:
		return D3D11_CULL_MODE::D3D11_CULL_BACK;
	case crossplatform::CullFaceMode::CULL_FACE_NONE:
		return D3D11_CULL_MODE::D3D11_CULL_NONE;
	default:
		return D3D11_CULL_MODE::D3D11_CULL_NONE;
		break;
	};
}

crossplatform::RenderState *RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	dx11on12::RenderState *s=new dx11on12::RenderState();
	s->type=desc.type;
	if(desc.type==crossplatform::BLEND)
	{
		D3D11_BLEND_DESC omDesc;
		ZeroMemory( &omDesc, sizeof( D3D11_BLEND_DESC ) );
		for(int i=0;i<desc.blend.numRTs;i++)
		{
			omDesc.RenderTarget[i].BlendEnable				=(desc.blend.RenderTarget[i].blendOperation!=crossplatform::BLEND_OP_NONE)||(desc.blend.RenderTarget[i].blendOperationAlpha!=crossplatform::BLEND_OP_NONE);
			omDesc.RenderTarget[i].BlendOp					=toD3dBlendOp(desc.blend.RenderTarget[i].blendOperation);
			omDesc.RenderTarget[i].SrcBlend					=toD3dBlend(desc.blend.RenderTarget[i].SrcBlend);
			omDesc.RenderTarget[i].DestBlend				=toD3dBlend(desc.blend.RenderTarget[i].DestBlend);
			omDesc.RenderTarget[i].BlendOpAlpha				=toD3dBlendOp(desc.blend.RenderTarget[i].blendOperationAlpha);
			omDesc.RenderTarget[i].SrcBlendAlpha			=toD3dBlend(desc.blend.RenderTarget[i].SrcBlendAlpha);
			omDesc.RenderTarget[i].DestBlendAlpha			=toD3dBlend(desc.blend.RenderTarget[i].DestBlendAlpha);
			omDesc.RenderTarget[i].RenderTargetWriteMask	=desc.blend.RenderTarget[i].RenderTargetWriteMask;
		}
		omDesc.IndependentBlendEnable			=desc.blend.IndependentBlendEnable;
		omDesc.AlphaToCoverageEnable			=desc.blend.AlphaToCoverageEnable;
		AsD3D11Device()->CreateBlendState(&omDesc,&s->m_blendState);
	}
	else if (desc.type == crossplatform::RASTERIZER)
	{
		D3D11_RASTERIZER_DESC rasterizerDesc;
		//Initialize the description of the stencil state.
		ZeroMemory(&rasterizerDesc,sizeof(rasterizerDesc));

		rasterizerDesc.FillMode				=toD3dFillMode(desc.rasterizer.polygonMode);
		rasterizerDesc.CullMode				=toD3dCullMode(desc.rasterizer.cullFaceMode);
		rasterizerDesc.FrontCounterClockwise=desc.rasterizer.frontFace==crossplatform::FrontFace::FRONTFACE_CLOCKWISE;
		rasterizerDesc.DepthBias			=0;
		rasterizerDesc.DepthBiasClamp		=0.0f;
		rasterizerDesc.SlopeScaledDepthBias	=0.0f;
		rasterizerDesc.DepthClipEnable		=false;
		rasterizerDesc.ScissorEnable		=(desc.rasterizer.viewportScissor==crossplatform::VIEWPORT_SCISSOR_ENABLE);
		rasterizerDesc.MultisampleEnable	=false;
		rasterizerDesc.AntialiasedLineEnable = false;
		AsD3D11Device()->CreateRasterizerState(&rasterizerDesc, &s->m_rasterizerState);
	}
	else if (desc.type == crossplatform::DEPTH)
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		//Initialize the description of the stencil state.
		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

		// Set up the description of the stencil state.
		depthStencilDesc.DepthEnable		=desc.depth.test;
		depthStencilDesc.DepthWriteMask		=desc.depth.write? D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc			= toD3dComparison(desc.depth.comparison);

		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;

		// Stencil operations if pixel is front-facing.
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		// Stencil operations if pixel is back-facing.
		depthStencilDesc.BackFace.StencilFailOp		= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp= D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp		= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc		= D3D11_COMPARISON_ALWAYS;
		
		AsD3D11Device()->CreateDepthStencilState(&depthStencilDesc, &s->m_depthStencilState);
	}
	return s;
}

crossplatform::Query *RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	dx11on12::Query *q=new dx11on12::Query(type);
	return q;
}

void *RenderPlatform::GetDevice()
{
	return device;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers
	,crossplatform::Buffer *const*buffers
	,const crossplatform::Layout *layout
	,const int *vertexSteps)
{
	UINT offset = 0;
	ID3D11Buffer *buf[10];
	UINT strides[10];
	UINT offsets[10];
	for(int i=0;i<num_buffers;i++)
	{
		strides[i]=buffers[i]->stride;
		if(vertexSteps&&vertexSteps[i]>=1)
			strides[i]*=vertexSteps[i];
		buf[i]=buffers[i]->AsD3D11Buffer();
		offsets[i]=0;
	}
	deviceContext.asD3D11DeviceContext()->IASetVertexBuffers(	0,	// the first input slot for binding
									num_buffers,					// the number of buffers in the array
									buf,							// the array of vertex buffers
									strides,						// array of stride values, one for each buffer
									offsets );						// array of offset values, one for each buffer
};

void RenderPlatform::SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *vertexBuffer,int start_index)
{
	ID3D11Buffer *b=NULL;
	if(vertexBuffer)
		b=vertexBuffer->AsD3D11Buffer();
	UINT offset = vertexBuffer?(vertexBuffer->stride*start_index):0;
	deviceContext.asD3D11DeviceContext()->SOSetTargets(1,&b,&offset );
}

void RenderPlatform::ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth)
{
	PushRenderTargets(deviceContext);
	ID3D11RenderTargetView *rt[8];
	SIMUL_ASSERT(num<=8);
	for(int i=0;i<num;i++)
		rt[i]=targs[i]->AsD3D11RenderTargetView();
	ID3D11DepthStencilView *d=NULL;
	if(depth)
		d=depth->AsD3D11DepthStencilView();
	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(num,rt,d);

	int w=targs[0]->width, h = targs[0]->length;
	crossplatform::Viewport v[] = { { 0, 0, w, h, 0, 1.f }, { 0, 0, w, h, 0, 1.f }, { 0, 0, w, h, 0, 1.f } };
	SetViewports(deviceContext,num,v);
}

void RenderPlatform::DeactivateRenderTargets(crossplatform::DeviceContext &deviceContext)
{
	PopRenderTargets(deviceContext);
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext &deviceContext,int num,const crossplatform::Viewport *vps)
{
	D3D11_VIEWPORT viewports[8];
	SIMUL_ASSERT(num<=8);
	for(int i=0;i<num;i++)
	{
		viewports[i].Width		=(float)vps[i].w;
		viewports[i].Height		=(float)vps[i].h;
		viewports[i].TopLeftX	=(float)vps[i].x;
		viewports[i].TopLeftY	=(float)vps[i].y;
		viewports[i].MinDepth	=vps[i].znear;
		viewports[i].MaxDepth	=vps[i].zfar;
	}
	deviceContext.asD3D11DeviceContext()->RSSetViewports(num,viewports);
}

crossplatform::Viewport	RenderPlatform::GetViewport(crossplatform::DeviceContext &deviceContext,int index)
{
	D3D11_VIEWPORT viewports[8];
	unsigned num=0;
	deviceContext.asD3D11DeviceContext()->RSGetViewports(&num,NULL);
	deviceContext.asD3D11DeviceContext()->RSGetViewports(&num,viewports);
	crossplatform::Viewport v;
	v.x=(int)viewports[index].TopLeftX;
	v.y=(int)viewports[index].TopLeftY;
	v.w=(int)viewports[index].Width;
	v.h=(int)viewports[index].Height;
	v.znear	=viewports[index].MinDepth;
	v.zfar	=viewports[index].MaxDepth;
	return v;
}

void RenderPlatform::SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer)
{
	if(!buffer)
	{
		deviceContext.asD3D11DeviceContext()->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
		return;
	}
	DXGI_FORMAT f;
	int index_byte_size=buffer->stride;
	switch(index_byte_size)
	{
	case 4:
		f=DXGI_FORMAT_R32_UINT;
		break;
	case 2:
		f=DXGI_FORMAT_R16_UINT;
		break;
	default:
		f=DXGI_FORMAT_UNKNOWN;
		SIMUL_BREAK("Can't use DXGI_FORMAT_UNKNOWN for an index buffer.")
		break;
	};
	deviceContext.asD3D11DeviceContext()->IASetIndexBuffer(buffer->AsD3D11Buffer(), f, 0);
}

static D3D11_PRIMITIVE_TOPOLOGY toD3dTopology(crossplatform::Topology t)
{
	using namespace crossplatform;
	switch(t)
	{			
	case POINTLIST:
		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case LINELIST:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case LINESTRIP:
		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case TRIANGLELIST:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case TRIANGLESTRIP:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case LINELIST_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
	case LINESTRIP_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
	case TRIANGLELIST_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
	case TRIANGLESTRIP_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
	default:
		return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	};
}

void RenderPlatform::SetTopology(crossplatform::DeviceContext &deviceContext,crossplatform::Topology t)
{
	D3D11_PRIMITIVE_TOPOLOGY T=toD3dTopology(t);
	deviceContext.asD3D11DeviceContext()->IASetPrimitiveTopology(T);
}

void RenderPlatform::SetLayout(crossplatform::DeviceContext &deviceContext,crossplatform::Layout *l)
{
	if(l)
		l->Apply(deviceContext);
	else
	{
		deviceContext.asD3D11DeviceContext()->IASetInputLayout(NULL);
	}
}

void RenderPlatform::SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s)
{
	if(!s)
		return;
	dx11on12::RenderState *ds=(dx11on12::RenderState *)(s);
	if(ds->type==crossplatform::BLEND)
	{
		float blendFactor[]		={1.0f,1.0f,1.0f,1.0f};
		UINT sampleMask			=0xffffffff;
		deviceContext.asD3D11DeviceContext()->OMSetBlendState(ds->m_blendState,blendFactor,sampleMask);
	}
	else if(ds->type==crossplatform::DEPTH)
	{
		deviceContext.asD3D11DeviceContext()->OMSetDepthStencilState(ds->m_depthStencilState,0);
	}
	else if(ds->type == crossplatform::RASTERIZER)
	{
		deviceContext.asD3D11DeviceContext()->RSSetState(ds->m_rasterizerState);
	}
}

void RenderPlatform::Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source)
{
	deviceContext.asD3D11DeviceContext()->ResolveSubresource(destination->AsD3D11Texture2D(),0,source->AsD3D11Texture2D(),0, dx11on12::RenderPlatform::ToDxgiFormat(destination->GetFormat()));
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
	dx11on12::SaveTexture(device,texture->AsD3D11Texture2D(),lFileNameUtf8);
}

bool RenderPlatform::ApplyContextState(crossplatform::DeviceContext &deviceContext, bool error_checking)
{
	crossplatform::ContextState *cs = GetContextState(deviceContext);
	if (!cs||!cs->currentEffectPass)
	{
		SIMUL_BREAK("No valid shader pass in ApplyContextState");
		return false;
	}
	ID3D11DeviceContext *d3d11DeviceContext = deviceContext.asD3D11DeviceContext();

	// NULL ptr here if we've not applied a valid shader..
	dx11on12::EffectPass *pass = static_cast<dx11on12::EffectPass*>(cs->currentEffectPass);
	Shader **sh = (dx11on12::Shader**)pass->shaders;
	// TODO: re-enable geometry shaders maybe
	if(sh[crossplatform::SHADERTYPE_GEOMETRY]||(sh[crossplatform::SHADERTYPE_VERTEX]!=nullptr&&sh[crossplatform::SHADERTYPE_PIXEL]==nullptr))
		return false;
	if (!cs->effectPassValid)
	{
		if (cs->last_action_was_compute&&pass->shaders[crossplatform::SHADERTYPE_VERTEX] != nullptr)
		{
			cs->last_action_was_compute = false;
		}
		else if ((!cs->last_action_was_compute) && pass->shaders[crossplatform::SHADERTYPE_COMPUTE] != nullptr)
		{
			cs->last_action_was_compute = true;
		}
		// This applies the pass, and also any associated state: Blend, Depth and Rasterizer states:
		pass->Apply(deviceContext, false);
		cs->effectPassValid = true;
	}
	if (!cs->samplerStateOverridesValid)
	{
		cs->samplerStateOverridesValid = true;
		for (auto i = cs->samplerStateOverrides.begin(); i != cs->samplerStateOverrides.end(); i++)
		{
			int slot = i->first;
			if (slot >= 0 && i->second>0)
			{
				auto c = i->second->asD3D11SamplerState();
				if (pass->shaders[crossplatform::SHADERTYPE_GEOMETRY])
				{
					deviceContext.asD3D11DeviceContext()->GSSetSamplers(slot, 1,&c);
				}
				else if (pass->shaders[crossplatform::SHADERTYPE_VERTEX])
					deviceContext.asD3D11DeviceContext()->VSSetSamplers(slot, 1,&c);
				if (pass->shaders[crossplatform::SHADERTYPE_PIXEL])
					deviceContext.asD3D11DeviceContext()->PSSetSamplers(slot, 1,&c);
				if (pass->shaders[crossplatform::SHADERTYPE_COMPUTE])
					deviceContext.asD3D11DeviceContext()->CSSetSamplers(slot, 1,&c);
			}
		}
	}
	// Apply buffers:
	if (!cs->buffersValid&&pass->usesBuffers())
	{
		cs->bufferSlots = 0;
		for (auto i = cs->applyBuffers.begin(); i != cs->applyBuffers.end(); i++)
		{
			int slot = i->first;
			if (slot != i->second->GetIndex())
			{
				SIMUL_BREAK_ONCE("This buffer assigned to the wrong index.");
			}
			
			i->second->GetPlatformConstantBuffer()->ActualApply(deviceContext, pass, i->second->GetIndex());
			if (error_checking&&pass->usesBufferSlot(slot))
				cs->bufferSlots |= (1 << slot);
		}
		if (error_checking)
		{
			unsigned required_buffer_slots = pass->GetConstantBufferSlots();
			if ((cs->bufferSlots&required_buffer_slots) != required_buffer_slots)
			{
				SIMUL_BREAK_ONCE("Not all constant buffers are assigned.");
				unsigned missing_slots = required_buffer_slots&(~cs->bufferSlots);
				for (unsigned i = 0; i<32; i++)
				{
					unsigned slot = 1 << i;
					if (slot&missing_slots)
						SIMUL_CERR << "Slot " << i << " was not set." << std::endl;
				}
			}
		}
		//else
		//	cs->buffersValid=true;
		// Constant buffers allocated from the command buffer, therefore never mark "valid",
		// because the old value will be consumed.
	}
	if (!cs->streamoutTargetsValid)
	{
		for (auto i = cs->streamoutTargets.begin(); i != cs->streamoutTargets.end(); i++)
		{
			int slot = i->first;
			dx11on12::Buffer *vertexBuffer = (dx11on12::Buffer*)i->second;
			if (!vertexBuffer)
				continue;

			ID3D11Buffer *b = vertexBuffer->AsD3D11Buffer();
			if (!b)
				continue;
			// TODO: nonzero offsets.
			unsigned offset = 0;
		/*	sce::Gnm::StreamoutBufferMapping bufferBinding;
			bufferBinding.init();
			bufferBinding.bindStream(sce::Gnm::kStreamoutBuffer0, sce::Gnm::StreamoutBufferMapping::kGsStreamBuffer0);
			sce::Gnmx::LightweightGfxContext *d3d11DeviceContext = deviceContext.asD3D11DeviceContext();
			// Setting partial vs wave, needed for streamout, the rest of the parameters are default values
			
			d3d11DeviceContext->setVgtControl(255, sce::Gnm::VgtPartialVsWaveMode::kVgtPartialVsWaveDisable);
			// Setting streamout parameters
			d3d11DeviceContext->flushStreamout();
			d3d11DeviceContext->setStreamoutMapping(&bufferBinding);
			unsigned bufferSizeDW = b->getSize() / 4;;
			unsigned bufferStrideDW = b->getStride() / 4;;
			d3d11DeviceContext->setStreamoutBufferDimensions(sce::Gnm::kStreamoutBuffer0, bufferSizeDW, bufferStrideDW);
			d3d11DeviceContext->writeStreamoutBufferOffset(sce::Gnm::kStreamoutBuffer0, 0);*/
		}
		cs->streamoutTargetsValid = true;
	}
	if (!cs->vertexBuffersValid)
	{
		for (auto i : cs->applyVertexBuffers)
		{
			//if(pass->UsesBufferSlot(i.first))
			dx11on12::Buffer* b = (dx11on12::Buffer*)(i.second);

			auto B=b->AsD3D11Buffer();
			d3d11DeviceContext->IASetVertexBuffers( i.first, b->count,&B,nullptr,nullptr);
		}
		cs->vertexBuffersValid = true;
	}
	cs->textureSlotsForSB = 0;
	cs->rwTextureSlotsForSB = 0;
	if (pass->usesSBs())
	{
		for (auto i = cs->applyStructuredBuffers.begin(); i != cs->applyStructuredBuffers.end(); i++)
		{
			int slot = i->first;
			if (!pass->usesTextureSlotForSB(slot))
				continue;
			i->second->ActualApply(deviceContext, pass, slot);
			if (slot<1000)
			{
				if (sh[crossplatform::SHADERTYPE_VERTEX] && sh[crossplatform::SHADERTYPE_VERTEX]->usesTextureSlotForSB(slot))
					cs->textureSlotsForSB |= (1 << slot);
				if (sh[crossplatform::SHADERTYPE_PIXEL] && sh[crossplatform::SHADERTYPE_PIXEL]->usesTextureSlotForSB(slot))
					cs->textureSlotsForSB |= (1 << slot);
				if (sh[crossplatform::SHADERTYPE_GEOMETRY] && sh[crossplatform::SHADERTYPE_GEOMETRY]->usesTextureSlotForSB(slot))
					cs->textureSlotsForSB |= (1 << slot);
				if (sh[crossplatform::SHADERTYPE_COMPUTE] && sh[crossplatform::SHADERTYPE_COMPUTE]->usesTextureSlotForSB(slot))
					cs->textureSlotsForSB |= (1 << slot);
			}
			else
			{
				if (sh[crossplatform::SHADERTYPE_COMPUTE] && sh[crossplatform::SHADERTYPE_COMPUTE]->usesRwTextureSlotForSB(slot - 1000))
					cs->rwTextureSlotsForSB |= (1 << (slot - 1000));
			}
		}
		if (error_checking)
		{
			unsigned required_sb_slots = pass->GetStructuredBufferSlots();
			if ((cs->textureSlotsForSB&required_sb_slots) != required_sb_slots)
			{
				SIMUL_BREAK_ONCE("Not all texture slots are assigned.");
			}
			unsigned required_rw_sb_slots = pass->GetRwStructuredBufferSlots();
			if ((cs->rwTextureSlotsForSB&required_rw_sb_slots) != required_rw_sb_slots)
			{
				SIMUL_BREAK_ONCE("Not all texture slots are assigned.");
			}
		}
	}
	// Apply textures:
	if (!cs->textureAssignmentMapValid&&pass->usesTextures())
	{
		cs->textureSlots = 0;
		cs->rwTextureSlots = 0;
		cs->textureAssignmentMapValid = true;
		for (auto i = cs->textureAssignmentMap.begin(); i != cs->textureAssignmentMap.end(); i++)
		{
			int slot = i->first;
			if (slot<0)
				continue;
			const crossplatform::TextureAssignment &ta = i->second;
			{
				if (!ta.texture || !ta.texture->IsValid())
				{
					// A NULL texture means we want default values. It would be NICE if DX would 
					// use its own documented behaviour:
					cs->textureSlots |= (1 << slot);
					continue;
				}
				Shader **sh = (Shader**)pass->shaders;
				if (ta.uav)
				{
					SIMUL_ASSERT_WARN_ONCE(slot >= 1000, "Bad slot number");
					if (sh[crossplatform::SHADERTYPE_COMPUTE]
						&& sh[crossplatform::SHADERTYPE_COMPUTE]->usesRwTextureSlot(slot - 1000))
					{
						auto T = ta.texture->AsD3D11UnorderedAccessView(ta.index, ta.mip);
						d3d11DeviceContext->CSSetUnorderedAccessViews( slot - 1000, 1, &T,0);
						cs->rwTextureSlots |= 1 << (slot - 1000);
					}
				}
				else
				{
					auto T = ta.texture->AsD3D11ShaderResourceView(ta.resourceType,ta.index, ta.mip);
					SIMUL_ASSERT_WARN_ONCE(slot<1000, "Bad slot number");
					if (sh[crossplatform::SHADERTYPE_GEOMETRY] && sh[crossplatform::SHADERTYPE_GEOMETRY]->usesTextureSlot(slot))
					{
						d3d11DeviceContext->GSSetShaderResources( slot, 1, &T);
						cs->textureSlots |= (1 << slot);
					}
					if (sh[crossplatform::SHADERTYPE_VERTEX] && sh[crossplatform::SHADERTYPE_VERTEX]->usesTextureSlot(slot))
					{
						d3d11DeviceContext->VSSetShaderResources( slot, 1, &T);
						cs->textureSlots |= (1 << slot);
					}
					Shader *p = sh[crossplatform::SHADERTYPE_PIXEL];
					if (p&&p->usesTextureSlot(slot))
					{
						d3d11DeviceContext->PSSetShaderResources( slot, 1, &T);
						cs->textureSlots |= (1 << slot);
					}
					if (sh[crossplatform::SHADERTYPE_COMPUTE] && sh[crossplatform::SHADERTYPE_COMPUTE]->usesTextureSlot(slot))
					{
						d3d11DeviceContext->CSSetShaderResources( slot, 1, &T);
						cs->textureSlots |= (1 << slot);
					}
				}
			}
		}
		// Now verify that ALL resource are set:
		if (error_checking)
		{
			unsigned required_slots = pass->GetTextureSlots();
			if ((cs->textureSlots&required_slots) != required_slots)
			{
				static int count = 10;
				count--;
				if (count>0)
				{
					SIMUL_CERR << "Not all texture slots are assigned:" << std::endl;
					unsigned missing_slots = required_slots&(~cs->textureSlots);
					for (unsigned i = 0; i<32; i++)
					{
						unsigned slot = 1 << i;
						if (slot&missing_slots)
						{
							std::string name;
							if (cs->currentEffect)
								name = cs->currentEffect->GetTextureForSlot(i);
							SIMUL_CERR << "\tSlot " << i << ": " << name.c_str() << ", was not set." << std::endl;
							missing_slots=missing_slots&(~slot);
							if(!missing_slots)
								break;
						}
					}
					//SIMUL_BREAK_ONCE("Many API's require all used textures to have valid data.");
				}
			}
			unsigned required_rw_slots = pass->GetRwTextureSlots();
			if ((cs->rwTextureSlots&required_rw_slots) != required_rw_slots)
			{
				static int count = 10;
				count--;
				if (count>0)
				{
					SIMUL_BREAK_ONCE("Not all rw texture slots are assigned.");
					required_rw_slots = required_rw_slots&(~cs->rwTextureSlots);
					for (unsigned i = 0; i<32; i++)
					{
						unsigned slot = 1 << i;
						if (slot&required_rw_slots)
						{
							std::string name;
							if (cs->currentEffect)
								name = cs->currentEffect->GetTextureForSlot(1000 + i);
							SIMUL_CERR << "RW Slot " << i << " was not set (" << name.c_str() << ")." << std::endl;
						}
					}
				}
			}
		}
	}
	return true;
}

#pragma optimize("",off)
void RenderPlatform::StoreRenderState( crossplatform::DeviceContext &deviceContext )
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	if(storedStateCursor>=storedStates.size())
	{
		storedStates.resize(std::max(1,storedStateCursor*2));
	}
	StoredState &s=storedStates[storedStateCursor++];
      pContext->OMGetDepthStencilState( &s.m_pDepthStencilStateStored11, &s.m_StencilRefStored11 );
    pContext->RSGetState(&s.m_pRasterizerStateStored11 );
  pContext->OMGetBlendState(&s.m_pBlendStateStored11,s.m_BlendFactorStored11, &s.m_SampleMaskStored11 );
    pContext->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, s.m_pSamplerStateStored11 );
    pContext->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT , s.m_pVertexSamplerStateStored11 );
	
    pContext->GSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT , s.m_pGeometrySamplerStateStored11 );
    pContext->CSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT , s.m_pComputeSamplerStateStored11 );

	pContext->VSGetShader(&s.pVertexShader,s.m_pVertexClassInstances,&s.numVertexClassInstances);
	pContext->PSGetShader(&s.pPixelShader,s.m_pPixelClassInstances,&s.numPixelClassInstances);
	pContext->HSGetShader(&s.pHullShader,s.m_pHullClassInstances,&s.numHullClassInstances);
	pContext->DSGetShader(&s.pDomainShader,s.m_pDomainClassInstances,&s.numDomainClassInstances);
	pContext->GSGetShader(&s.pGeometryShader,s.m_pGeometryClassInstances,&s.numGeometryClassInstances);
	pContext->CSGetShader(&s.pComputeShader,s.m_pPixelClassInstances,&s.numPixelClassInstances);
	
	pContext->CSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pCSConstantBuffers);
	pContext->GSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pGSConstantBuffers);
	pContext->PSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pPSConstantBuffers);
	pContext->VSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pVSConstantBuffers);
	pContext->HSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pHSConstantBuffers);
	pContext->DSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pDSConstantBuffers);
	
	pContext->PSGetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pShaderResourceViews);
	pContext->CSGetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pCSShaderResourceViews);
	pContext->CSGetUnorderedAccessViews(0,D3D11_PS_CS_UAV_REGISTER_COUNT,s.m_pUnorderedAccessViews);
		 
	pContext->IAGetInputLayout( &s.m_previousInputLayout );
	pContext->IAGetPrimitiveTopology(&s.m_previousTopology);

	pContext->IAGetVertexBuffers(	0,
									D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
									s.m_pVertexBuffersStored11,
									s.m_VertexStrides,
									s.m_VertexOffsets	);

	pContext->IAGetIndexBuffer(&s.pIndexBufferStored11,
								&s.m_indexFormatStored11,
								&s.m_indexOffset);
}

void RenderPlatform::RestoreRenderState( crossplatform::DeviceContext &deviceContext )
{
	storedStateCursor--;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	StoredState &s=storedStates[storedStateCursor];
    pContext->OMSetDepthStencilState(s.m_pDepthStencilStateStored11,s.m_StencilRefStored11 );
    SAFE_RELEASE(s.m_pDepthStencilStateStored11 );
	pContext->RSSetState(s.m_pRasterizerStateStored11 );
    pContext->OMSetBlendState(s.m_pBlendStateStored11,s.m_BlendFactorStored11,s.m_SampleMaskStored11 );
    pContext->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pSamplerStateStored11 );
    pContext->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pVertexSamplerStateStored11 );
    pContext->CSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pComputeSamplerStateStored11 );
    pContext->GSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pGeometrySamplerStateStored11 );
	pContext->VSSetShader(s.pVertexShader,s.m_pVertexClassInstances,s.numVertexClassInstances);
	pContext->PSSetShader(s.pPixelShader,s.m_pPixelClassInstances,s.numPixelClassInstances);
	pContext->HSSetShader(s.pHullShader,s.m_pHullClassInstances,s.numHullClassInstances);
	pContext->DSSetShader(s.pDomainShader,s.m_pDomainClassInstances,s.numDomainClassInstances);
	pContext->GSSetShader(s.pGeometryShader,s.m_pGeometryClassInstances,s.numGeometryClassInstances);
	pContext->CSSetShader(s.pComputeShader,s.m_pPixelClassInstances,s.numPixelClassInstances);
	
    SAFE_RELEASE(s.pVertexShader );
    SAFE_RELEASE(s.pPixelShader );
	SAFE_RELEASE(s.pVertexShader);
	SAFE_RELEASE(s.pPixelShader);
	SAFE_RELEASE(s.pHullShader);
	SAFE_RELEASE(s.pDomainShader);
	SAFE_RELEASE(s.pGeometryShader);
	SAFE_RELEASE(s.pComputeShader);
	
	pContext->CSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pCSConstantBuffers);
	pContext->GSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pGSConstantBuffers);
	pContext->PSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pPSConstantBuffers);
	pContext->VSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pVSConstantBuffers);
	pContext->HSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pHSConstantBuffers);
	pContext->DSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pDSConstantBuffers);
	
	for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
	{
		SAFE_RELEASE(s.m_pCSConstantBuffers[i]);
		SAFE_RELEASE(s.m_pGSConstantBuffers[i]);
		SAFE_RELEASE(s.m_pPSConstantBuffers[i]);
		SAFE_RELEASE(s.m_pVSConstantBuffers[i]);
		SAFE_RELEASE(s.m_pHSConstantBuffers[i]);
		SAFE_RELEASE(s.m_pDSConstantBuffers[i]);
	}

	pContext->PSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pShaderResourceViews);
	pContext->CSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pCSShaderResourceViews);
	for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
	{
		SAFE_RELEASE(s.m_pShaderResourceViews[i]);
		SAFE_RELEASE(s.m_pCSShaderResourceViews[i]);
	}
	// TODO: handle produce-consume buffers below
	pContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, s.m_pUnorderedAccessViews, NULL);
	for (int i = 0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		SAFE_RELEASE(s.m_pUnorderedAccessViews[i]);
	}

    SAFE_RELEASE(s.m_pRasterizerStateStored11 );
    SAFE_RELEASE(s.m_pBlendStateStored11 );
	for(int i=0;i<D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;i++)
	{
	    SAFE_RELEASE(s.m_pSamplerStateStored11[i]);
	    SAFE_RELEASE(s.m_pVertexSamplerStateStored11[i]);
	    SAFE_RELEASE(s.m_pComputeSamplerStateStored11[i]);
	    SAFE_RELEASE(s.m_pGeometrySamplerStateStored11[i]);
	}
	pContext->IASetPrimitiveTopology(s.m_previousTopology);
	pContext->IASetInputLayout(s.m_previousInputLayout );
	SAFE_RELEASE(s.m_previousInputLayout);

	pContext->IASetVertexBuffers(	0,
									D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
									s.m_pVertexBuffersStored11,
									s.m_VertexStrides,
									s.m_VertexOffsets	);
	for(int i=0;i<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;i++)
	{
	    SAFE_RELEASE(s.m_pVertexBuffersStored11[i]);
	}

	pContext->IASetIndexBuffer(s.pIndexBufferStored11,
								s.m_indexFormatStored11,
								s.m_indexOffset);
	SAFE_RELEASE(s.pIndexBufferStored11);
	
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend)
{
	crossplatform::RenderPlatform::DrawTexture(deviceContext, x1, y1, dx, dy, tex, mult, blend);
	//if(!tex)
//		DrawTexture(deviceContext,x1,y1,dx,dy,(ID3D11ShaderResourceView*)NULL,mult,blend);
	//else
		//DrawTexture(deviceContext,x1,y1,dx,dy,tex->AsD3D11ShaderResourceView(),mult,blend);
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(NULL);
	ApplyContextState(deviceContext);
	pContext->Draw(4,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *line_vertices,int vertex_count,bool strip,bool test_depth,bool view_centred)
{
	if(!vertex_count)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	{
		HRESULT hr=S_OK;
		crossplatform::EffectTechniqueGroup *g=debugEffect->GetTechniqueGroupByName(test_depth?"lines_3d_depth":"lines_3d");
		crossplatform::Frustum f=crossplatform::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
			crossplatform::EffectTechnique *tech=g->GetTechniqueByIndex(0);
		if(test_depth)
			tech=g->GetTechniqueByName(f.reverseDepth?"depth_reverse":"depth_forward");
		simul::math::Matrix4x4 wvp;
		if(view_centred)
		{
			crossplatform::MakeCentredViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
			debugConstants.debugWorldViewProj=wvp;
			debugConstants.debugWorldViewProj.transpose();
		}
		else
		{
			crossplatform::MakeViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
			debugConstants.debugWorldViewProj=wvp;
			debugConstants.debugWorldViewProj.transpose();
		}
		debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
		ID3D11Buffer *					vertexBuffer=NULL;
		// Create the vertex buffer:
		D3D11_BUFFER_DESC desc=
		{
			vertex_count*sizeof(crossplatform::PosColourVertex),
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_VERTEX_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
			0
		};
		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
		InitData.pSysMem = line_vertices;
		InitData.SysMemPitch = sizeof(crossplatform::PosColourVertex);
		hr=AsD3D11Device()->CreateBuffer(&desc,&InitData,&vertexBuffer);

		const D3D11_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		D3DX11_PASS_DESC PassDesc;
		ID3DX11EffectPass *pass=tech->asD3DX11EffectTechnique()->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);

		if(!m_pVtxDecl)
			hr=AsD3D11Device()->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
		ID3D11InputLayout* previousInputLayout;
		pContext->IAGetInputLayout( &previousInputLayout );
		pContext->IASetInputLayout(m_pVtxDecl);
		D3D_PRIMITIVE_TOPOLOGY previousTopology;
		pContext->IAGetPrimitiveTopology(&previousTopology);
		pContext->IASetPrimitiveTopology(strip?D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		UINT stride = sizeof(crossplatform::PosColourVertex);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = stride;
		Offsets[0] = 0;
		pContext->IASetVertexBuffers(	0,							// the first input slot for binding
													1,				// the number of buffers in the array
													&vertexBuffer,	// the array of vertex buffers
													&stride,		// array of stride values, one for each buffer
													&offset);		// array of 
		hr=ApplyPass(pContext,tech->asD3DX11EffectTechnique()->GetPassByIndex(0));
	ApplyContextState(deviceContext);
		pContext->Draw(vertex_count,0);
		pContext->IASetPrimitiveTopology(previousTopology);
		pContext->IASetInputLayout( previousInputLayout );
		SAFE_RELEASE(previousInputLayout);
		SAFE_RELEASE(vertexBuffer);
	}
}

void RenderPlatform::Draw2dLines(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int vertex_count,bool strip)
{
	//if(!vertex_count)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	{
		HRESULT hr=S_OK;
		ID3DX11EffectTechnique *tech			=debugEffect->asD3DX11Effect()->GetTechniqueByName("lines_2d");
		
		unsigned int num_v=1;
		//D3D11_VIEWPORT								viewport;
		//pContext->RSGetViewports(&num_v,&viewport);
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
		debugConstants.rect=vec4(-1.f,-1.f,2.f,2.f);//-1.0,-1.0,2.0f/viewport.Width,2.0f/viewport.Height);
		debugEffect->SetConstantBuffer(deviceContext,&debugConstants);

		ID3D11Buffer *vertexBuffer=NULL;
		// Create the vertex buffer:
		D3D11_BUFFER_DESC desc=
		{
			vertex_count*sizeof(VertexXyzRgba),
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_VERTEX_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
			0
		};
		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
		InitData.pSysMem = lines;
		InitData.SysMemPitch = sizeof(VertexXyzRgba);
		hr=AsD3D11Device()->CreateBuffer(&desc,&InitData,&vertexBuffer);

		const D3D11_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		D3DX11_PASS_DESC PassDesc;
		ID3DX11EffectPass *pass=tech->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);

		ID3D11InputLayout*				m_pVtxDecl=NULL;
		SAFE_RELEASE(m_pVtxDecl);
		hr=AsD3D11Device()->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
		pContext->IASetInputLayout(m_pVtxDecl);
		ID3D11InputLayout* previousInputLayout;
		pContext->IAGetInputLayout( &previousInputLayout );
		D3D_PRIMITIVE_TOPOLOGY previousTopology;
		pContext->IAGetPrimitiveTopology(&previousTopology);
		pContext->IASetPrimitiveTopology(strip?D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		UINT stride = sizeof(VertexXyzRgba);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = stride;
		Offsets[0] = 0;
		pContext->IASetVertexBuffers(	0,				// the first input slot for binding
										1,				// the number of buffers in the array
										&vertexBuffer,	// the array of vertex buffers
										&stride,		// array of stride values, one for each buffer
										&offset);		// array of 
		hr=ApplyPass(pContext,tech->GetPassByIndex(0));
	ApplyContextState(deviceContext);
		pContext->Draw(vertex_count,0);
		pContext->IASetPrimitiveTopology(previousTopology);
		pContext->IASetInputLayout( previousInputLayout );
		SAFE_RELEASE(previousInputLayout);
		SAFE_RELEASE(vertexBuffer);
		SAFE_RELEASE(m_pVtxDecl);
	}
}

void RenderPlatform::DrawCube(crossplatform::DeviceContext &deviceContext)
{
	if(!m_pCubemapVtxDecl)
		return;
	UINT stride = sizeof(vec3);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout( &previousInputLayout );

	pContext->IASetVertexBuffers(	0,					// the first input slot for binding
									1,					// the number of buffers in the array
									&m_pVertexBuffer,	// the array of vertex buffers
									&stride,			// array of stride values, one for each buffer
									&offset );			// array of offset values, one for each buffer

	// Set the input layout
	pContext->IASetInputLayout(m_pCubemapVtxDecl);

	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ApplyContextState(deviceContext);
	pContext->Draw(36,0);

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
}

namespace simul
{
	namespace dx11on12
	{
		struct RTState
		{
			RTState():depthSurface(NULL),numViewports(0)
			{
				memset(renderTargets,0,sizeof(renderTargets));
				memset(viewports,0,sizeof(viewports));
			}
			ID3D11RenderTargetView*				renderTargets[16];
			ID3D11DepthStencilView*				depthSurface;
			D3D11_VIEWPORT						viewports[16];
			unsigned							numViewports;
		};
	}
}

void RenderPlatform::PushRenderTargets(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	RTState *state=new RTState;
	pContext->RSGetViewports(&state->numViewports,NULL);
	if(state->numViewports>0)
		pContext->RSGetViewports(&state->numViewports,state->viewports);
	pContext->OMGetRenderTargets(	state->numViewports,
									state->renderTargets,
									&state->depthSurface);
	storedRTStates.push_back(state);
}

void RenderPlatform::PopRenderTargets(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	RTState *state=storedRTStates.back();
	pContext->OMSetRenderTargets(	state->numViewports,
									state->renderTargets,
									state->depthSurface);
	for(int i=0;i<(int)state->numViewports;i++)
	{
		SAFE_RELEASE(state->renderTargets[i]);
	}
	SAFE_RELEASE(state->depthSurface);
	if(state->numViewports>0)
		pContext->RSSetViewports(state->numViewports,state->viewports);
	delete state;
	storedRTStates.pop_back();
}

crossplatform::Shader *RenderPlatform::CreateShader()
{
	Shader *S = new Shader();
	return S;
}