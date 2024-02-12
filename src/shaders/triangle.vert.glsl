#version 450

layout(push_constant) uniform uniforms_block
{
	mat4 view_matrix;
};

layout(location=0) in vec3 pos;
layout(location=1) in vec2 tex_coord;

layout(location= 0) out vec4 f_color;
layout(location= 1) out vec2 f_tex_coord;

void main()
{
	f_tex_coord= tex_coord;
	gl_Position= view_matrix * vec4(pos, 1.0);
}
