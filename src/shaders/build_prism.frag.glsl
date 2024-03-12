#version 450

layout(location = 0) in float f_alpha;
layout(location = 1) in vec4 f_stripes;

layout(location = 0) out vec4 out_color;

void main()
{
	float f= fract(dot(vec4(0.5), step(0.5, fract(f_stripes)))) * 2.0;
	out_color= vec4(vec3(0.0, 0.0, 0.0), f_alpha * f);
}
