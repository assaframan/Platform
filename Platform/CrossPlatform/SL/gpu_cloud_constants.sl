#ifndef SIMUL_CROSSPLATFORM_GPU_CLOUD_CONSTANTS_SL
#define SIMUL_CROSSPLATFORM_GPU_CLOUD_CONSTANTS_SL

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

#endif