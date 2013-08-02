#ifndef TERRAIN_CONSTANTS_SL
#define TERRAIN_CONSTANTS_SL

uniform_buffer TerrainConstants R10
{
	uniform mat4 worldViewProj;

	uniform vec4 eyePosition;
	uniform vec3 lightDir;
	uniform float pad1;

	uniform vec3 sunlight;
	uniform float pad2;
	uniform vec3 ambientColour;
	uniform float pad3;
	uniform vec3 cloudScales;	
	uniform float cloudInterp;
	uniform vec3 cloudOffset;
	uniform float morphFactor;
};

#endif