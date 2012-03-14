// simul_stars.frag - a GLSL fragment shader
// Copyright 2011 Simul Software Ltd

varying vec2 texcoord;
uniform float starBrightness;
varying vec3 pos;


void main(void)
{
	vec3 colour=vec3(1.0,1.0,1.0)*clamp(starBrightness*texcoord.x,0.0,1.0);
	if(pos.z<0)
		colour*=0;
    gl_FragColor=vec4(colour,1.0);
}

 