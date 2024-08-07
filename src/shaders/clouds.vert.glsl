#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/sky_shader_uniforms.glsl"

// Use for clouds mesh a pyramid with square base.

const float c_pyramid_half_width= 1024.0;
const float c_pyramid_height= 2048.0;
const float c_pyramid_depth= -256.0;

const vec3 c_vertices[5]= vec3[5]
(
	vec3( c_pyramid_half_width,  c_pyramid_half_width, c_pyramid_depth),
	vec3(-c_pyramid_half_width,  c_pyramid_half_width, c_pyramid_depth),
	vec3( c_pyramid_half_width, -c_pyramid_half_width, c_pyramid_depth),
	vec3(-c_pyramid_half_width, -c_pyramid_half_width, c_pyramid_depth),
	vec3(0, 0, c_pyramid_height)
);

const int c_num_indices= 4 * 3;

const int c_indeces[c_num_indices]= int[c_num_indices]
(
	4, 1, 0,
	4, 3, 1,
	4, 2, 3,
	4, 0, 2
);

layout(binding= 0) uniform uniforms_block_variable
{
	SkyShaderUniforms uniforms;
};

layout(location= 0) out vec3 f_view_vec;

void main()
{
	vec3 pos= c_vertices[c_indeces[gl_VertexIndex]];
	f_view_vec= pos;
	gl_Position= uniforms.view_matrix * vec4(pos, 1.0);
}
