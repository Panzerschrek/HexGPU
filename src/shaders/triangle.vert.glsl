#version 450

layout(push_constant) uniform uniforms_block
{
	mat4 view_matrix;
};

layout(location=0) in vec3 pos;
layout(location=1) in vec4 tex_coord;

layout(location= 0) out vec4 f_color;
layout(location= 1) out vec2 f_tex_coord;
layout(location= 2) out flat float f_tex_index;

// Texture coordnates are initialiiy somewhat scaled. Rescale them back.
const vec2 c_tex_coord_scale= vec2(1.0 / 4.0, 1.0 / 4.0);

void main()
{
	f_tex_coord= tex_coord.xy * c_tex_coord_scale;
	f_tex_index= tex_coord.z;
	gl_Position= view_matrix * vec4(pos, 1.0);
}
