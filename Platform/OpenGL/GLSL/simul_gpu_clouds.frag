#version 140
#include "../Glsl.h"
#include "../../CrossPlatform/simul_gpu_clouds.sl"
uniform sampler2D input_light_texture;
uniform sampler3D density_texture;

varying vec2 in_texcoord;

void main(void)
{
	vec2 texcoord				=in_texcoord.xy;//+texCoordOffset;
	vec4 previous_light			=texture(input_light_texture,texcoord.xy);
	vec3 lightspace_texcoord	=vec3(texcoord.xy,zPosition);
	vec3 densityspace_texcoord	=(vec4(lightspace_texcoord,1.0)*transformMatrix).xyz;
	float density				=texture(density_texture,densityspace_texcoord).x;
	float direct_light			=previous_light.x*exp(-extinctions.x*density);
	float indirect_light		=previous_light.y*exp(-extinctions.y*density);

    gl_FragColor				=vec4(direct_light,indirect_light,0.0,0.0);
}
