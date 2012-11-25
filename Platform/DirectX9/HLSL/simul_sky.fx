float4x4 worldViewProj : WorldViewProjection;

texture inscTexture;

sampler2D insc_texture = sampler_state
{
    Texture = <inscTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Mirror;
	AddressV = Clamp;
};

texture skylTexture;

sampler2D skyl_texture = sampler_state
{
    Texture = <skylTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Mirror;
	AddressV = Clamp;
};

texture starsTexture;
sampler2D stars_texture = sampler_state
{
    Texture = <starsTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Mirror;
};

texture flareTexture;
sampler flare_texture = sampler_state
{
    Texture = <flareTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};


texture fadeTexture;
sampler3D fade_texture = sampler_state
{
    Texture = <fadeTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};
texture fadeTexture2;
sampler3D fade_texture_2 = sampler_state
{
    Texture = <fadeTexture2>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};
texture fadeTexture2D;
sampler2D fade_texture_2d= sampler_state
{
    Texture = <fadeTexture2D>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Mirror;
	AddressV = Clamp;
};

//------------------------------------
// Parameters 
//------------------------------------
float4 eyePosition : EYEPOSITION_WORLDSPACE;
float4 lightDir : Direction;
float4 mieRayleighRatio;
float hazeEccentricity;
float skyInterp;
float altitudeTexCoord;
#define pi (3.1415926536f)

float4 colour;
float starBrightness=.1f;
float3 texelOffset;
float3 texelScale;
//------------------------------------
// Structures 
//------------------------------------
struct vertexInput
{
    float3 position			: POSITION;
};

struct vertexOutput
{
    float4 hPosition		: POSITION;
    float3 wDirection		: TEXCOORD1;
};

struct vertexInputCS
{
    float3 position			: POSITION;
    float4 colour			: COLOR;
    float2 texCoords		: TEXCOORD0;
};

struct vertexOutputCS
{
    float4 hPosition		: POSITION;
    float3 texCoords		: TEXCOORD0;
    float4 colour			: TEXCOORD2;
};
struct vertexInput3Dto2D
{
    float3 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexOutput3Dto2D
{
    float4 hPosition		: POSITION;
    float2 texCoords		: TEXCOORD0;
};
//------------------------------------
// Vertex Shader 
//------------------------------------
vertexOutput VS_Main(vertexInput IN) 
{
    vertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.hPosition.z=OUT.hPosition.w;
    OUT.wDirection=normalize(IN.position.xyz);
    return OUT;
}

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.f+g2-2.f*g*cos0;
	return 0.5*0.079577+0.5*(1.f-g2)/(4.f*pi*sqrt(u*u*u));
}

float3 InscatterFunction(float4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831f*(1.f+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 result=BetaTotal*inscatter_factor.rgb;
	return result;
}

float4 PS_Main( vertexOutput IN): COLOR
{
	float3 view=normalize(IN.wDirection.xyz);
#ifdef Y_VERTICAL
	float sine	=view.y;
#else
	float sine	=view.z;
#endif
	float2 texcoord	=float2(1.0,0.5*(1.0-sine));
	float4 insc_factor	=tex2D(insc_texture,texcoord);
	float4 skyl			=tex2D(skyl_texture,texcoord);
	float cos0=dot(lightDir.xyz,view.xyz);
	float3 result=InscatterFunction(insc_factor,cos0);
	result.rgb+=skyl.rgb;
	return float4(result.rgb,1.0);
}

float4 PS_Stars(vertexOutput IN): COLOR
{
	float3 view=normalize(IN.wDirection.xyz);
#ifdef Y_VERTICAL
	float sine	=view.y;
	float azimuth=atan2(view.x,view.z);
#else
	float sine	=view.z;
	float azimuth=atan2(view.x,view.y);
#endif
	float elev=asin(sine);
	float2 stars_texcoord=float2(azimuth/(2.0*pi),0.5*(1.f-elev/(pi/2.0)));
	float3 output=starBrightness*tex2D(stars_texture,stars_texcoord).rgb;
	return float4(output,1.f);
}

struct svertexInput
{
    float3 position			: POSITION;
    float2 tex				: TEXCOORD0;
};

struct svertexOutput
{
    float4 hPosition		: POSITION;
    float2 tex				: TEXCOORD0;
};

svertexOutput VS_Sun(svertexInput IN) 
{
    svertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.tex=IN.position.xy;
    return OUT;
}

// Sun could be overbright. So the colour is in the range [0,1], and a brightness factor is
// stored in the alpha channel.
float4 PS_Sun(svertexOutput IN): color
{
	float r=length(IN.tex);
	if(r>1.0)
		discard;
	float brightness=saturate((1.0-r)/0.1)+colour.a*saturate((0.9-r)/0.1);
	float3 result=brightness*colour.rgb;
	return float4(result,1.f);
}

float4 PS_Flare(svertexOutput IN): color
{
	float3 output=colour.rgb*tex2D(flare_texture,float2(0.5f,0.5f)+0.5f*IN.tex).rgb;
	return float4(output,1.f);
}

float approx_oren_nayar(float roughness,float3 view,float3 normal,float3 lightDir)
{
	float roughness2 = roughness * roughness;
	float2 oren_nayar_fraction = roughness2 / (roughness2 + float2(0.33, 0.09));
	float2 oren_nayar = float2(1, 0) + float2(-0.5, 0.45) * oren_nayar_fraction;
	// Theta and phi
	float2 cos_theta = saturate(float2(dot(normal, lightDir), dot(normal, view)));
	float2 cos_theta2 = cos_theta * cos_theta;
	float u=saturate((1-cos_theta2.x) * (1-cos_theta2.y));
	float sin_theta = sqrt(u);
	float3 light_plane = normalize(lightDir - cos_theta.x * normal);
	float3 view_plane = normalize(view - cos_theta.y * normal);
	float cos_phi = saturate(dot(light_plane, view_plane));
	// Composition
	float diffuse_oren_nayar = cos_phi * sin_theta / max(0.00001,max(cos_theta.x, cos_theta.y));
	float diffuse = cos_theta.x * (oren_nayar.x + oren_nayar.y * diffuse_oren_nayar);
	return diffuse;
}

float4 PS_Planet(svertexOutput IN): color
{
	float4 result=tex2D(flare_texture,float2(0.5f,0.5f)-0.5f*IN.tex);
	// IN.tex is +- 1.
	float3 normal;
	normal.x=IN.tex.x;
	normal.y=IN.tex.y;
	float l=length(IN.tex);
	if(l>1.0)
		discard;
	normal.z=-sqrt(1.f-l*l);
	float light=approx_oren_nayar(0.2,float3(0,0,1.0),normal,lightDir);
	result.rgb*=colour.rgb;
	result.rgb*=light;
	result.a*=saturate((0.99-l)/0.01);
	return result;
}

float4 PS_Query(svertexOutput IN): color
{
	return float4(1.f,0.f,0.f,0.f);
}

svertexOutput VS_Point_Stars(svertexInput IN) 
{
    svertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.tex=IN.tex;
    return OUT;
}

float4 PS_Point_Stars(svertexOutput IN): color
{
	float3 result=float3(1.f,1.f,1.f)*saturate(starBrightness*IN.tex.x);
	return float4(result,1.f);
}


vertexOutputCS VS_ShowFade(vertexInputCS IN)
{
    vertexOutputCS OUT;
    OUT.hPosition = mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.texCoords.xy=IN.texCoords.xy;
	OUT.texCoords.z=1;
	OUT.colour=IN.colour;
    return OUT;
}

float4 PS_ShowFade( vertexOutputCS IN): color
{
	IN.texCoords.y=1.0-IN.texCoords.y;
	float4 result=tex2D(fade_texture_2d,IN.texCoords.xy);
    return float4(result.rgb,1);
}

float4 PS_ShowSkyTexture( vertexOutputCS IN): color
{
	float4 result=tex2D(fade_texture_2d,float2(IN.texCoords.y,altitudeTexCoord));
    return float4(result.rgb,1);
}


vertexOutputCS VS_CrossSection(vertexInputCS IN)
{
    vertexOutputCS OUT;
    OUT.hPosition = mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.texCoords.yz=IN.texCoords.yx;
	OUT.texCoords.x=altitudeTexCoord;
	OUT.colour=IN.colour;
    return OUT;
}

float4 PS_CrossSectionXZ( vertexOutputCS IN): color
{
	float3 texc=float3(altitudeTexCoord,IN.texCoords.yx);
	float4 result=tex3D(fade_texture,texc);
    return float4(result.rgb,1);
}

vertexOutput3Dto2D VS_3D_to_2D(vertexInput3Dto2D IN)
{
    vertexOutput3Dto2D OUT;
    OUT.hPosition =float4(IN.position,1.f);
	OUT.texCoords=IN.texCoords;
    return OUT;
}

float4 PS_3D_to_2D(vertexOutput3Dto2D IN): color
{
	float3 texc=float3(altitudeTexCoord,IN.texCoords.yx)*texelScale+texelOffset;
	float4 colour1=tex3D(fade_texture,texc);
	float4 colour2=tex3D(fade_texture_2,texc);
	float4 result=lerp(colour1,colour2,skyInterp);
    return result;
}

struct vertexInputPosTex
{
    float3 position			: POSITION;
    float4 texCoords		: TEXCOORD0;
};

struct vertexOutputPosTex
{
    float4 hPosition		: POSITION;
    float4 texCoords		: TEXCOORD0;
};

vertexOutputPosTex VS_PointStars(vertexInputPosTex IN)
{
    vertexOutputPosTex OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.hPosition.z=OUT.hPosition.w;
    OUT.texCoords=IN.texCoords;
    return OUT;
}

float4 PS_PointStars(vertexOutputPosTex IN): color
{
	float3 result=starBrightness*float3(1.f,1.f,1.f)*IN.texCoords.x;
	return float4(result,1.f);
}

//------------------------------------
// Technique 
//------------------------------------
technique simul_sky
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_0 PS_Main();
       
        CullMode = None;
		zenable = true;
		zwriteenable = false;
      //  AlphaBlendEnable = false;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_starry_sky
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_b PS_Stars();
       
        CullMode = None;
		zenable = true;
		zwriteenable = false;
        AlphaBlendEnable = false;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_sun
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Sun();
		PixelShader  = compile ps_2_0 PS_Sun();
       
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_planet
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Sun();
		PixelShader  = compile ps_2_0 PS_Planet();
       
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_flare
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Sun();
		PixelShader  = compile ps_2_0 PS_Flare();
       
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_query
{
    pass p0 
    {		
		VertexShader = compile vs_3_0 VS_Sun();
		PixelShader  = compile ps_3_0 PS_Query();
		zenable = true;
		zwriteenable = false;
        AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_show_fade
{
    pass p0 
    {		
		VertexShader = compile vs_3_0 VS_ShowFade();
		PixelShader  = compile ps_3_0 PS_ShowFade();
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = false;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_show_sky_texture
{
    pass p0 
    {		
		VertexShader = compile vs_3_0 VS_ShowFade();
		PixelShader  = compile ps_3_0 PS_ShowSkyTexture();
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = false;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_fade_cross_section_xz
{
    pass p0 
    {		
		VertexShader = compile vs_3_0 VS_CrossSection();
		PixelShader  = compile ps_3_0 PS_CrossSectionXZ();
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = false;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_fade_3d_to_2d
{
    pass p0 
    {		
		VertexShader = compile vs_3_0 VS_3D_to_2D();
		PixelShader  = compile ps_3_0 PS_3D_to_2D();
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = false;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_point_stars
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_PointStars();
		PixelShader  = compile ps_2_b PS_PointStars();
       
        CullMode = None;
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = false;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}
