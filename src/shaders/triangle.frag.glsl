#version 450

layout(binding= 1) uniform sampler2D texture_image;

layout(location= 0) in vec4 f_color;
layout(location= 1) in vec2 f_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color= texture(texture_image, f_tex_coord);
}
