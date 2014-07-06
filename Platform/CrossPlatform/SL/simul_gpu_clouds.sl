#ifndef SIMUL_GPU_CLOUDS_SL
#define SIMUL_GPU_CLOUDS_SL

uniform_buffer GpuCloudConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 transformMatrix;
	uniform vec4 yRange;
	uniform vec3 noiseScale;
	uniform float noiseDimsZ;
	uniform vec2 extinctions;
	uniform float stepLength,ttt;
	uniform uint3 gaussianOffset;
	uniform int octaves;
	uniform uint3 threadOffset;
	uniform float zPosition;

	uniform float time;
	uniform float persistence;
	uniform float humidity;
	uniform float zPixelLightspace;

	uniform float zPixel;
	uniform float zSize;
	uniform float baseLayer;
	uniform float transition;

	uniform float upperDensity;
	uniform float diffusivity;
	uniform float invFractalSum;
	uniform float maskThickness;

	uniform vec2 maskCentre;
	uniform float maskRadius;
	uniform float maskFeather;
};

#ifndef __cplusplus

uint3 LinearThreadToPos3D(uint linear_pos,uint3 dims)
{
	uint Z				=linear_pos/dims.x/dims.y;
	uint Y				=(linear_pos-Z*dims.x*dims.y)/dims.x;
	uint X				=linear_pos-Y*(dims.x+Z*dims.y);
	uint3 pos			=uint3(X,Y,Z);
	return pos;
}

vec3 assemble3dTexcoord(vec2 texcoord2)
{
	vec2 texcoord	=texcoord2.xy;
	texcoord.y		*=zSize;
	float zPos		=floor(texcoord.y)/zSize+0.5*zPixel;
	texcoord.y		=fract(texcoord.y);
	vec3 texcoord3	=vec3(texcoord.xy,zPos);
	return texcoord3;
}

float GetHumidityMultiplier(float z)
{
	// i is the lerp variable between the top of the base layer and the upper layer.
#if 1
	float i	=smoothstep(baseLayer,baseLayer+transition,z);
	float m	=lerp(1.0,upperDensity,i);
	float j	=1.0-saturate((1.0-z)/transition);
	m		*=sqrt(1.0-j*j);
#else
	float i	=saturate((z-baseLayer)/transition);
	float m	=lerp(1.0,upperDensity,i*i);
	float j	=1.0-saturate((1.0-z)/transition);
	m		*=lerp(1.0,0.75,j*j);
#endif
	return m;
}
//
float CircularLookup(Texture3D volumeNoiseTexture,vec3 texCoords,float sz,float t,int scale,int top)
{
	vec3 s		=frac(texCoords)*scale;
	//..s.xyz		+=texture_wrap_lod(volumeNoiseTexture,texCoords,0).xyz-0.5;
	int3 pos3	=trunc(s);
	vec3 offs	=frac(s);
	int3 poss[8]=
	{
		{0,0,0},
		{1,0,0},
		{0,1,0},
		{1,1,0},
		{0,0,1},
		{1,0,1},
		{0,1,1},
		{1,1,1},
	};
	float lookup=0.0;
	for(int i=0;i<8;i++)
	{
		vec3 corner	=poss[i];
		vec3 dist3	=offs-corner;
		float dist	=length(dist3);
		float mult	=saturate(sz-dist/(0.866));
		if(mult>0)
		{
			int3 p		=(pos3+poss[i]);
			p			=p%scale;
			if(p.z>=top)
				continue;
			float val	=volumeNoiseTexture.Load(int4(p,0));
			lookup		+=mult*(1.0+cos(2.0*3.1415926536*(val+t)));
		}
	}
	return saturate(lookup);//texture_wrap_nearest_lod(volumeNoiseTexture,texCoords,0);
}

float PowerLookup(Texture3D volumeNoiseTexture,vec3 texCoords,float t)
{
	float val	=texture_wrap_lod(volumeNoiseTexture,texCoords,0);
	return pow(val,0.25);
}

// height is the height of the total cloud volume as a proportion of the initial noise volume
float NoiseFunction(Texture3D volumeNoiseTexture,vec3 pos,int octaves,float persistence,float t,float height,float texel)
{
	float dens=0.0;
	float mult=persistence;
	float sum=0.0;
	int noiseDimension=4;
	int noiseHeight=12;
	for(int i=1;i<5;i++)
	{
		if(i>=octaves)
			break;
		vec3 pos2		=pos;
		// We will limit the z-value of pos2 in order to prevent unwanted blending to out-of-range texels.
	/*	float zmin		=0.5*texel;
		float zmax		=height-0.5*texel;
		pos2.z		=clamp(pos2.z,zmin,zmax);
		pos2.z		*=saturate(i);

		pos2.z			-=0.1;*/
		float lookup	=CircularLookup(volumeNoiseTexture,pos2,1.0,t,noiseDimension,noiseHeight)-0.5;
		dens			+=mult*lookup;
		sum				+=mult;
		mult			*=persistence;
		pos				=pos*2.0;
		t				*=2.0;
		height			*=2.0;
		noiseDimension	*=2;
		noiseHeight		*=2;
	//	if(noiseDimension>noiseDimsZ)
	//		noiseDimension=noiseDimsZ;
	}
	//dens=(dens/sum);
	return dens;
}
float DensityFunction(Texture3D volumeNoiseTexture,vec3 noisespace_texcoord,float hum,float t)
{
	int noiseDimension=4;
	int noiseHeight=4;
	noisespace_texcoord.xy		+=0.125*texture_wrap_lod(volumeNoiseTexture,vec3(noisespace_texcoord.xy,0),0).xy-0.5;
	float lookup	=CircularLookup(volumeNoiseTexture,vec3(noisespace_texcoord.xy,0),hum,t,noiseDimension,noiseHeight);
	return lookup;
}

float GpuCloudMask(vec2 texCoords,vec2 maskCentre,float maskRadius,float maskFeather,float maskThickness)
{
    float dr	=maskFeather;
    vec2 pos	=texCoords.xy-maskCentre;
    float r		=length(pos)/maskRadius;
    float dens	=maskThickness*saturate((1.0-r)/dr);
    return dens;
}

#ifndef GLSL
void CS_CloudDensity(RWTexture3D<float4> targetTexture,Texture3D volumeNoiseTexture,Texture2D maskTexture,uint3 sub_pos)
{
	uint3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	uint3 noise_dims;
	volumeNoiseTexture.GetDimensions(noise_dims.x,noise_dims.y,noise_dims.z);
	uint3 pos					=sub_pos+threadOffset;
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	vec3 densityspace_texcoord	=(pos+vec3(0.5,0.5,0.5))/vec3(dims);
	vec3 noisespace_texcoord	=(densityspace_texcoord+vec3(0,0,0.0*zPixel))*noiseScale+vec3(1.0,1.0,0);
	// noise_texel is the size of a noise texel
	float noise_texel			=1.0/noise_dims.z;
	float height				=noiseScale.z;
	float hm					=humidity*GetHumidityMultiplier(densityspace_texcoord.z)*texture_clamp_lod(maskTexture,densityspace_texcoord.xy,0).x;
	float main_density			=DensityFunction(volumeNoiseTexture,noisespace_texcoord,hm,time);
	float dens					=main_density+NoiseFunction(volumeNoiseTexture,noisespace_texcoord,octaves,persistence,time,height,noise_texel);
	dens				=saturate((dens+hm-1.0)/diffusivity);
	//dens					=saturate((dens-0.5)/diffusivity);
	dens						*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
	dens						=saturate(dens);
	targetTexture[pos]			=dens;
}
#endif

#if 0
vec4 PS_CloudDensity(Texture3D volumeNoiseTexture,Texture2D maskTexture,vec2 texCoords,float humidity,float diffusivity,int octaves,float persistence,float time,float zPixel)
{
	vec3 densityspace_texcoord	=assemble3dTexcoord(texCoords.xy);
	vec3 noisespace_texcoord	=densityspace_texcoord*noiseScale+vec3(1.0,1.0,0);
	float noise_texel			=1.0/noise_dims.z;
	float height				=noiseScale;
	float noise_val				=NoiseFunction(volumeNoiseTexture,noisespace_texcoord,octaves,persistence,time,height,noise_texel);
	float hm					=humidity*GetHumidityMultiplier(densityspace_texcoord.z)*texture_clamp(maskTexture,densityspace_texcoord.xy).x;
	float dens					=saturate((noise_val+hm-1.0)/diffusivity);
	dens						*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
    return vec4(dens,0,0,1.0);
}
#endif
#endif
#endif
