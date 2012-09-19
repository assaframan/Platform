
#include "CreateEffectDX1x.h"

#include <assert.h>
#include "ocean_simulator.h"

// Disable warning "conditional expression is constant"
#pragma warning(disable:4127)


#define HALF_SQRT_2	0.7071068f
#define GRAV_ACCEL	981.0f	// The acceleration of gravity, cm/s^2

#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 16

// Generating gaussian random number with mean 0 and standard deviation 1.
float Gauss()
{
	float u1 = rand() / (float)RAND_MAX;
	float u2 = rand() / (float)RAND_MAX;
	if (u1 < 1e-6f)
		u1 = 1e-6f;
	return sqrtf(-2 * logf(u1)) * cosf(2*D3DX_PI * u2);
}

// Phillips Spectrum
// K: normalized wave vector, W: wind direction, v: wind velocity, a: amplitude constant
float Phillips(D3DXVECTOR2 K, D3DXVECTOR2 W, float v, float a, float dir_depend)
{
	// largest possible wave from constant wind of velocity v
	float l = v * v / GRAV_ACCEL;
	// damp out waves with very small length w << l
	float w = l / 1000;

	float Ksqr = K.x * K.x + K.y * K.y;
	float Kcos = K.x * W.x + K.y * W.y;
	float phillips = a * expf(-1 / (l * l * Ksqr)) / (Ksqr * Ksqr * Ksqr) * (Kcos * Kcos);

	// filter out waves moving opposite to wind
	if (Kcos < 0)
		phillips *= dir_depend;

	// damp out waves with very small length w << l
	return phillips * expf(-Ksqr * w * w);
}

void createBufferAndUAV(ID3D11Device* pd3dDevice, void* data, UINT byte_width, UINT byte_stride,
						ID3D11Buffer** ppBuffer, ID3D11UnorderedAccessView** ppUAV, ID3D11ShaderResourceView** ppSRV)
{
	// Create buffer
	D3D11_BUFFER_DESC buf_desc;
	buf_desc.ByteWidth = byte_width;
    buf_desc.Usage = D3D11_USAGE_DEFAULT;
    buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    buf_desc.CPUAccessFlags = 0;
    buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    buf_desc.StructureByteStride = byte_stride;

	D3D11_SUBRESOURCE_DATA init_data = {data, 0, 0};

	pd3dDevice->CreateBuffer(&buf_desc, data != NULL ? &init_data : NULL, ppBuffer);
	assert(*ppBuffer);

	// Create undordered access view
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = byte_width / byte_stride;
	uav_desc.Buffer.Flags = 0;

	pd3dDevice->CreateUnorderedAccessView(*ppBuffer, &uav_desc, ppUAV);
	assert(*ppUAV);

	// Create shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.FirstElement = 0;
	srv_desc.Buffer.NumElements = byte_width / byte_stride;

	pd3dDevice->CreateShaderResourceView(*ppBuffer, &srv_desc, ppSRV);
	assert(*ppSRV);
}

void createTextureAndViews(ID3D11Device* pd3dDevice, UINT width, UINT height, DXGI_FORMAT format,
						   ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV, ID3D11RenderTargetView** ppRTV)
{
	// Create 2D texture
	D3D11_TEXTURE2D_DESC tex_desc;
	tex_desc.Width = width;
	tex_desc.Height = height;
	tex_desc.MipLevels = 0;
	tex_desc.ArraySize = 1;
	tex_desc.Format = format;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	pd3dDevice->CreateTexture2D(&tex_desc, NULL, ppTex);
	assert(*ppTex);

	// Create shader resource view
	(*ppTex)->GetDesc(&tex_desc);
	if (ppSRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
		srv_desc.Texture2D.MostDetailedMip = 0;

		pd3dDevice->CreateShaderResourceView(*ppTex, &srv_desc, ppSRV);
		assert(*ppSRV);
	}

	// Create render target view
	if (ppRTV)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
		rtv_desc.Format = format;
		rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = 0;

		pd3dDevice->CreateRenderTargetView(*ppTex, &rtv_desc, ppRTV);
		assert(*ppRTV);
	}
}

OceanSimulator::OceanSimulator(OceanParameter& params, ID3D11Device* pd3dDevice)
{
	// If the device becomes invalid at some point, delete current instance and generate a new one.
	assert(pd3dDevice);
	pd3dDevice->GetImmediateContext(&m_pd3dImmediateContext);
	assert(m_pd3dImmediateContext);

	// Height map H(0)
	int height_map_size = (params.dmap_dim + 4) * (params.dmap_dim + 1);
	D3DXVECTOR2* h0_data = new D3DXVECTOR2[height_map_size * sizeof(D3DXVECTOR2)];
	float* omega_data = new float[height_map_size * sizeof(float)];
	initHeightMap(params, h0_data, omega_data);

	m_param = params;
	int hmap_dim = params.dmap_dim;
	int input_full_size = (hmap_dim + 4) * (hmap_dim + 1);
	// This value should be (hmap_dim / 2 + 1) * hmap_dim, but we use full sized buffer here for simplicity.
	int input_half_size = hmap_dim * hmap_dim;
	int output_size = hmap_dim * hmap_dim;

	// For filling the buffer with zeroes.
	char* zero_data = new char[3 * output_size * sizeof(float) * 2];
	memset(zero_data, 0, 3 * output_size * sizeof(float) * 2);

	// RW buffer allocations
	// H0
	UINT float2_stride = 2 * sizeof(float);
	createBufferAndUAV(pd3dDevice, h0_data, input_full_size * float2_stride, float2_stride, &m_pBuffer_Float2_H0, &m_pUAV_H0, &m_pSRV_H0);

	// Notice: The following 3 buffers should be half sized buffer because of conjugate symmetric input. But
	// we use full sized buffers due to the CS4.0 restriction.

	// Put H(t), Dx(t) and Dy(t) into one buffer because CS4.0 allows only 1 UAV at a time
	createBufferAndUAV(pd3dDevice, zero_data, 3 * input_half_size * float2_stride, float2_stride, &m_pBuffer_Float2_Ht, &m_pUAV_Ht, &m_pSRV_Ht);

	// omega
	createBufferAndUAV(pd3dDevice, omega_data, input_full_size * sizeof(float), sizeof(float), &m_pBuffer_Float_Omega, &m_pUAV_Omega, &m_pSRV_Omega);

	// Notice: The following 3 should be real number data. But here we use the complex numbers and C2C FFT
	// due to the CS4.0 restriction.
	// Put Dz, Dx and Dy into one buffer because CS4.0 allows only 1 UAV at a time
	createBufferAndUAV(pd3dDevice, zero_data, 3 * output_size * float2_stride, float2_stride, &m_pBuffer_Float_Dxyz, &m_pUAV_Dxyz, &m_pSRV_Dxyz);

	SAFE_DELETE_ARRAY(zero_data);
	SAFE_DELETE_ARRAY(h0_data);
	SAFE_DELETE_ARRAY(omega_data);

	// Have now created: H0, Ht, Omega, Dxyz - as both UAV's and SRV's.

	// D3D11 Textures - these ar the outputs of the ocean simulator.
	createTextureAndViews(pd3dDevice, hmap_dim, hmap_dim, DXGI_FORMAT_R32G32B32A32_FLOAT, &m_pDisplacementMap, &m_pDisplacementSRV, &m_pDisplacementRTV);
	createTextureAndViews(pd3dDevice, hmap_dim, hmap_dim, DXGI_FORMAT_R16G16B16A16_FLOAT, &m_pGradientMap, &m_pGradientSRV, &m_pGradientRTV);

	// Sampler state for no filtering:
	D3D11_SAMPLER_DESC sam_desc;
	sam_desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.MipLODBias = 0; 
	sam_desc.MaxAnisotropy = 1; 
	sam_desc.ComparisonFunc = D3D11_COMPARISON_NEVER; 
	sam_desc.BorderColor[0] = 1.0f;
	sam_desc.BorderColor[1] = 1.0f;
	sam_desc.BorderColor[2] = 1.0f;
	sam_desc.BorderColor[3] = 1.0f;
	sam_desc.MinLOD = -FLT_MAX;
	sam_desc.MaxLOD = FLT_MAX;
	pd3dDevice->CreateSamplerState(&sam_desc, &m_pPointSamplerState);
	assert(m_pPointSamplerState);

	// Create the compute shader: UpdateSpectrumCS
    ID3DBlob* pBlobUpdateSpectrumCS = NULL;
    CompileShaderFromFile(L"ocean_simulator_cs.hlsl", "UpdateSpectrumCS", "cs_4_0", &pBlobUpdateSpectrumCS);
    pd3dDevice->CreateComputeShader(pBlobUpdateSpectrumCS->GetBufferPointer(),pBlobUpdateSpectrumCS->GetBufferSize(),NULL,&m_pUpdateSpectrumCS);  
    SAFE_RELEASE(pBlobUpdateSpectrumCS);

	// Create the vertex shader:
    ID3DBlob* pBlobQuadVS = NULL;
    CompileShaderFromFile(L"ocean_simulator_vs_ps.hlsl", "QuadVS", "vs_4_0", &pBlobQuadVS);
    pd3dDevice->CreateVertexShader(pBlobQuadVS->GetBufferPointer(), pBlobQuadVS->GetBufferSize(), NULL, &m_pQuadVS);
	// Create the vertex shader's input layout:
	D3D11_INPUT_ELEMENT_DESC quad_layout_desc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	pd3dDevice->CreateInputLayout(quad_layout_desc, 1, pBlobQuadVS->GetBufferPointer(), pBlobQuadVS->GetBufferSize(), &m_pQuadLayout);
	SAFE_RELEASE(pBlobQuadVS);
	
	// Create the pixel shaders:
    ID3DBlob* pBlobUpdateDisplacementPS = NULL;
	CompileShaderFromFile(L"ocean_simulator_vs_ps.hlsl", "UpdateDisplacementPS", "ps_4_0", &pBlobUpdateDisplacementPS);
    pd3dDevice->CreatePixelShader(pBlobUpdateDisplacementPS->GetBufferPointer(), pBlobUpdateDisplacementPS->GetBufferSize(), NULL, &m_pUpdateDisplacementPS);
	SAFE_RELEASE(pBlobUpdateDisplacementPS);

	ID3DBlob* pBlobGenGradientFoldingPS = NULL;
    CompileShaderFromFile(L"ocean_simulator_vs_ps.hlsl", "GenGradientFoldingPS", "ps_4_0", &pBlobGenGradientFoldingPS);
    pd3dDevice->CreatePixelShader(pBlobGenGradientFoldingPS->GetBufferPointer(), pBlobGenGradientFoldingPS->GetBufferSize(), NULL, &m_pGenGradientFoldingPS);
	SAFE_RELEASE(pBlobGenGradientFoldingPS);

	// Create YET ANOTHER fullscreen quad vertex buffer - because DirectX can't just do this in a single line of code!
	D3D11_BUFFER_DESC vb_desc;
	vb_desc.ByteWidth = 4 * sizeof(D3DXVECTOR4);
	vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb_desc.CPUAccessFlags = 0;
	vb_desc.MiscFlags = 0;

	float quad_verts[] =
	{
		-1, -1, 0, 1,
		-1,  1, 0, 1,
		 1, -1, 0, 1,
		 1,  1, 0, 1,
	};
	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = &quad_verts[0];
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;
	pd3dDevice->CreateBuffer(&vb_desc, &init_data, &m_pQuadVB);

	// Constant buffers
	UINT actual_dim = m_param.dmap_dim;

	// Compare this to the definition in the shaders.
	struct cbImmutable
	{
		UINT g_ActualDim;
		UINT g_InWidth;
		UINT g_OutWidth;
		UINT g_OutHeight;
		UINT g_DxAddressOffset;
		UINT g_DyAddressOffset;
	};
	cbImmutable immutable;
	// We use full sized data here. The value "output_width" should be actual_dim/2+1 though.
	immutable.g_ActualDim		=actual_dim;
	immutable.g_InWidth			=actual_dim + 4;
	immutable.g_OutWidth		=actual_dim;
	immutable.g_OutHeight		=actual_dim;
	immutable.g_DxAddressOffset	=actual_dim*actual_dim;
	immutable.g_DyAddressOffset	=actual_dim*actual_dim*2;

	D3D11_SUBRESOURCE_DATA init_cb0 = {&immutable, 0, 0};

	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.MiscFlags = 0;    
	cb_desc.ByteWidth = PAD16(sizeof(immutable));
	pd3dDevice->CreateBuffer(&cb_desc, &init_cb0, &m_pImmutableCB);
	assert(m_pImmutableCB);

	// Assign this constant buffer to both the compute shader and the pixel shader.
	ID3D11Buffer* cbs[1] = {m_pImmutableCB};
	m_pd3dImmediateContext->CSSetConstantBuffers(0, 1, cbs);
	m_pd3dImmediateContext->PSSetConstantBuffers(0, 1, cbs);

	cb_desc.Usage = D3D11_USAGE_DYNAMIC;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags = 0;    
	cb_desc.ByteWidth = PAD16(sizeof(float) * 3);
	pd3dDevice->CreateBuffer(&cb_desc, NULL, &m_pPerFrameCB);
	assert(m_pPerFrameCB);

	// FFT
	//fft512x512_create_plan(&m_fft, pd3dDevice, 3);
	m_fft.RestoreDeviceObjects(pd3dDevice, 3);
#ifdef CS_DEBUG_BUFFER
	D3D11_BUFFER_DESC buf_desc;
	buf_desc.ByteWidth = 3 * input_half_size * float2_stride;
    buf_desc.Usage = D3D11_USAGE_STAGING;
    buf_desc.BindFlags = 0;
    buf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    buf_desc.StructureByteStride = float2_stride;

	pd3dDevice->CreateBuffer(&buf_desc, NULL, &m_pDebugBuffer);
	assert(m_pDebugBuffer);
#endif
}

OceanSimulator::~OceanSimulator()
{
	SAFE_RELEASE(m_pBuffer_Float2_H0);
	SAFE_RELEASE(m_pBuffer_Float_Omega);
	SAFE_RELEASE(m_pBuffer_Float2_Ht);
	SAFE_RELEASE(m_pBuffer_Float_Dxyz);

	SAFE_RELEASE(m_pPointSamplerState);

	SAFE_RELEASE(m_pQuadVB);

	SAFE_RELEASE(m_pUAV_H0);
	SAFE_RELEASE(m_pUAV_Omega);
	SAFE_RELEASE(m_pUAV_Ht);
	SAFE_RELEASE(m_pUAV_Dxyz);

	SAFE_RELEASE(m_pSRV_H0);
	SAFE_RELEASE(m_pSRV_Omega);
	SAFE_RELEASE(m_pSRV_Ht);
	SAFE_RELEASE(m_pSRV_Dxyz);

	SAFE_RELEASE(m_pDisplacementMap);
	SAFE_RELEASE(m_pDisplacementSRV);
	SAFE_RELEASE(m_pDisplacementRTV);

	SAFE_RELEASE(m_pGradientMap);
	SAFE_RELEASE(m_pGradientSRV);
	SAFE_RELEASE(m_pGradientRTV);

	SAFE_RELEASE(m_pUpdateSpectrumCS);
	SAFE_RELEASE(m_pQuadVS);
	SAFE_RELEASE(m_pUpdateDisplacementPS);
	SAFE_RELEASE(m_pGenGradientFoldingPS);

	SAFE_RELEASE(m_pQuadLayout);

	SAFE_RELEASE(m_pImmutableCB);
	SAFE_RELEASE(m_pPerFrameCB);

	SAFE_RELEASE(m_pd3dImmediateContext);

#ifdef CS_DEBUG_BUFFER
	SAFE_RELEASE(m_pDebugBuffer);
#endif
}


// Initialize the vector field.
// wlen_x: width of wave tile, in meters
// wlen_y: length of wave tile, in meters
void OceanSimulator::initHeightMap(OceanParameter& params, D3DXVECTOR2* out_h0, float* out_omega)
{
	int i, j;
	D3DXVECTOR2 K, Kn;

	D3DXVECTOR2 wind_dir;
	D3DXVec2Normalize(&wind_dir, &params.wind_dir);
	float a = params.wave_amplitude * 1e-7f;	// It is too small. We must scale it for editing.
	float v = params.wind_speed;
	float dir_depend = params.wind_dependency;

	int height_map_dim = params.dmap_dim;
	float patch_length = params.patch_length;

	// initialize random generator.
	srand(0);

	for (i = 0; i <= height_map_dim; i++)
	{
		// K is wave-vector, range [-|DX/W, |DX/W], [-|DY/H, |DY/H]
		K.y = (-height_map_dim / 2.0f + i) * (2 * D3DX_PI / patch_length);

		for (j = 0; j <= height_map_dim; j++)
		{
			K.x = (-height_map_dim / 2.0f + j) * (2 * D3DX_PI / patch_length);

			float phil = (K.x == 0 && K.y == 0) ? 0 : sqrtf(Phillips(K, wind_dir, v, a, dir_depend));

			out_h0[i * (height_map_dim + 4) + j].x = float(phil * Gauss() * HALF_SQRT_2);
			out_h0[i * (height_map_dim + 4) + j].y = float(phil * Gauss() * HALF_SQRT_2);

			// The angular frequency is following the dispersion relation:
			//            out_omega^2 = g*k
			// The equation of Gerstner wave:
			//            x = x0 - K/k * A * sin(dot(K, x0) - sqrt(g * k) * t), x is a 2D vector.
			//            z = A * cos(dot(K, x0) - sqrt(g * k) * t)
			// Gerstner wave shows that a point on a simple sinusoid wave is doing a uniform circular
			// motion with the center (x0, y0, z0), radius A, and the circular plane is parallel to
			// vector K.
			out_omega[i * (height_map_dim + 4) + j] = sqrtf(GRAV_ACCEL * sqrtf(K.x * K.x + K.y * K.y));
		}
	}
}

void OceanSimulator::updateDisplacementMap(float time)
{
	// ---------------------------- H(0) -> H(t), D(x, t), D(y, t) --------------------------------
	// Compute shader
	m_pd3dImmediateContext->CSSetShader(m_pUpdateSpectrumCS, NULL, 0);

	// Buffers
	ID3D11ShaderResourceView* cs0_srvs[2] = {m_pSRV_H0, m_pSRV_Omega};
	m_pd3dImmediateContext->CSSetShaderResources(0, 2, cs0_srvs);

	ID3D11UnorderedAccessView* cs0_uavs[1] = {m_pUAV_Ht};
	m_pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));

	// Consts
	D3D11_MAPPED_SUBRESOURCE mapped_res;            
	m_pd3dImmediateContext->Map(m_pPerFrameCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
	assert(mapped_res.pData);
	float* per_frame_data = (float*)mapped_res.pData;
	// g_Time
	per_frame_data[0] = time * m_param.time_scale;
	// g_ChoppyScale
	per_frame_data[1] = m_param.choppy_scale;
	// g_GridLen
	per_frame_data[2] = m_param.dmap_dim / m_param.patch_length;
	m_pd3dImmediateContext->Unmap(m_pPerFrameCB, 0);

	ID3D11Buffer* cs_cbs[2] = {m_pImmutableCB, m_pPerFrameCB};
	m_pd3dImmediateContext->CSSetConstantBuffers(0, 2, cs_cbs);

	// Run the CS
	UINT group_count_x = (m_param.dmap_dim + BLOCK_SIZE_X - 1) / BLOCK_SIZE_X;
	UINT group_count_y = (m_param.dmap_dim + BLOCK_SIZE_Y - 1) / BLOCK_SIZE_Y;
	m_pd3dImmediateContext->Dispatch(group_count_x, group_count_y, 1);

	// Unbind resources for CS
	cs0_uavs[0] = NULL;
	m_pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));
	cs0_srvs[0] = NULL;
	cs0_srvs[1] = NULL;
	m_pd3dImmediateContext->CSSetShaderResources(0, 2, cs0_srvs);


	// Perform Fast (inverse) Fourier Transform from the source Ht to the destination Dxyz.
	// NOTE: we also provide the SRV of Dxyz so that FFT can use it as a temporary buffer and save space.
	m_fft.fft_512x512_c2c(m_pUAV_Dxyz,m_pSRV_Dxyz,m_pSRV_Ht);

	// Now we will use the transformed Dxyz to create our displacement map
	// --------------------------------- Wrap Dx, Dy and Dz ---------------------------------------
	// Save the current RenderTarget and viewport:
	ID3D11RenderTargetView* old_target;
	ID3D11DepthStencilView* old_depth;
	m_pd3dImmediateContext->OMGetRenderTargets(1, &old_target, &old_depth); 
	D3D11_VIEWPORT old_viewport;
	UINT num_viewport = 1;
	m_pd3dImmediateContext->RSGetViewports(&num_viewport, &old_viewport);

	// Set the new viewport as equal to the size of the displacement texture we're writing to:
	D3D11_VIEWPORT new_vp = {0, 0, (float)m_param.dmap_dim, (float)m_param.dmap_dim, 0.0f, 1.0f};
	m_pd3dImmediateContext->RSSetViewports(1, &new_vp);

	// Set the RenderTarget as the displacement map:
	ID3D11RenderTargetView* rt_views[1] = {m_pDisplacementRTV};
	m_pd3dImmediateContext->OMSetRenderTargets(1, rt_views, NULL);

	// Assign the shaders:
	m_pd3dImmediateContext->VSSetShader(m_pQuadVS, NULL, 0);
	m_pd3dImmediateContext->PSSetShader(m_pUpdateDisplacementPS, NULL, 0);

	// Assign the constant-buffers to the pixel shader:
	ID3D11Buffer* ps_cbs[2] = {m_pImmutableCB, m_pPerFrameCB};
	m_pd3dImmediateContext->PSSetConstantBuffers(0, 2, ps_cbs);

	// Assign the Dxyz source as a resource for the pixel shader:
	ID3D11ShaderResourceView* ps_srvs[1] = {m_pSRV_Dxyz};
    m_pd3dImmediateContext->PSSetShaderResources(0, 1, ps_srvs);

	// Setup the input layout - all this just to draw a quad:
	ID3D11Buffer* vbs[1] = {m_pQuadVB};
	UINT strides[1] = {sizeof(D3DXVECTOR4)};
	UINT offsets[1] = {0};
	m_pd3dImmediateContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);
	m_pd3dImmediateContext->IASetInputLayout(m_pQuadLayout);
	m_pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Perform draw call
	m_pd3dImmediateContext->Draw(4, 0);

	// Unbind the shader resource (i.e. the input texture map). Must do this or we get a DX warning when we
	// try to write to the texture again:
	ps_srvs[0] = NULL;
    m_pd3dImmediateContext->PSSetShaderResources(0, 1, ps_srvs);


	// Now generate the gradient map.
	// Set the gradient texture as the RenderTarget:
	rt_views[0] = m_pGradientRTV;
	m_pd3dImmediateContext->OMSetRenderTargets(1, rt_views, NULL);

	// VS & PS
	m_pd3dImmediateContext->VSSetShader(m_pQuadVS, NULL, 0);
	m_pd3dImmediateContext->PSSetShader(m_pGenGradientFoldingPS, NULL, 0);

	// Use the Displacement map as the texture input:
	ps_srvs[0] = m_pDisplacementSRV;
    m_pd3dImmediateContext->PSSetShaderResources(0, 1, ps_srvs);

	// Set the sampler state for point (i.e. unfiltered) rendering - don't want smoothing for this operation:
	ID3D11SamplerState* samplers[1] = {m_pPointSamplerState};
	m_pd3dImmediateContext->PSSetSamplers(0, 1, &samplers[0]);

	// Perform draw call
	m_pd3dImmediateContext->Draw(4, 0);

	// Unbind the shader resource (the texture):
	ps_srvs[0] = NULL;
    m_pd3dImmediateContext->PSSetShaderResources(0, 1, ps_srvs);

	// Reset the renderTarget to what it was before:
	m_pd3dImmediateContext->RSSetViewports(1, &old_viewport);
	m_pd3dImmediateContext->OMSetRenderTargets(1, &old_target, old_depth);
	SAFE_RELEASE(old_target);
	SAFE_RELEASE(old_depth);

	// Make mipmaps for the gradient texture, apparently this is a quick operation:
	m_pd3dImmediateContext->GenerateMips(m_pGradientSRV);
}

ID3D11ShaderResourceView* OceanSimulator::getDisplacementMap()
{
	return m_pDisplacementSRV;
}

ID3D11ShaderResourceView* OceanSimulator::getGradientMap()
{
	return m_pGradientSRV;
}


const OceanParameter& OceanSimulator::getParameters()
{
	return m_param;
}