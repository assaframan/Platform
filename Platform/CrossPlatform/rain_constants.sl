#ifndef RAIN_CONSTANTS_SL
#define RAIN_CONSTANTS_SL

uniform_buffer RainConstants SIMUL_BUFFER_REGISTER(10)
{
	uniform vec4 lightColour;
	uniform vec3 lightDir;
	uniform float snowSize;
	uniform float offset;
	uniform float intensity;
	uniform float flurry,flurryRate;
	uniform float phase;
};

uniform_buffer RainPerViewConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 worldViewProj[2];
	uniform mat4 invViewProj[2];
	uniform vec3 viewPos[2];
	uniform float filld;
	uniform vec2 tanHalfFov;
	uniform float nearZ;
	uniform float farZ;
	uniform vec3 depthToLinFadeDistParams;
	uniform float nearRainDistance;			// as a proportion of max fade distance
	uniform vec4 viewportToTexRegionScaleBias;
};
uniform_buffer RainOsdConstants SIMUL_BUFFER_REGISTER(9)
{
	uniform vec4 rect;
};

struct TransformedParticle
{
    vec4 position;
	float pointSize;
	float brightness;
	vec3 view;
};

#endif