#version 450

layout(location= 0) in vec4 f_color;
layout(location= 1) in vec2 f_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color= vec4(fract(f_tex_coord), 0.0, 1.0);
}
