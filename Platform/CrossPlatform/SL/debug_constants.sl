#ifndef DEBUG_CONSTANTS_SL
#define DEBUG_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(DebugConstants,8)
	uniform mat4 debugWorldViewProj;
	uniform vec4 rect;

	uniform int latitudes;
	uniform int longitudes;
	uniform float radius;
	uniform float multiplier;

	uniform vec4 viewport;
	uniform vec4 colour;
	uniform vec4 debugDepthToLinFadeDistParams;

	uniform vec2 debugTanHalfFov;
	uniform float debugExposure;
	uniform float debugGamma;
SIMUL_CONSTANT_BUFFER_END
#endif
