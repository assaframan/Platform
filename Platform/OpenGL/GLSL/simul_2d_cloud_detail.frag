uniform sampler2D noise_texture;
uniform float persistence;
varying vec2 texc;

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}

vec4 saturate(vec4 x)
{
	return clamp(x,vec4(0.0,0.0,0.0,0.0),vec4(1.0,1.0,1.0,1.0));
}

void main(void)
{
	vec4 result=vec4(0,0,0,0);
	vec2 texcoords=texc;
	float mul=.5;
    for(int i=0;i<24;i++)
    {
		vec4 c=texture(noise_texture,texcoords);
		texcoords*=2.0;
		texcoords+=mul*vec2(0.2,0.2)*c.xy;
		result+=mul*c;
		mul*=persistence;
    }
    result.rgb=vec3(1.0,1.0,1.0);//=saturate(result*1.5);
	result.a=saturate(result.a-0.4)/0.4;
    gl_FragColor=result;
}
