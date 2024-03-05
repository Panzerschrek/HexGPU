#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"

const float c_skybox_half_size= 16.0;

const vec3 c_sky_vertices[8]= vec3[8]
(
	vec3( c_skybox_half_size,  c_skybox_half_size,  c_skybox_half_size),
	vec3(-c_skybox_half_size,  c_skybox_half_size,  c_skybox_half_size),
	vec3( c_skybox_half_size, -c_skybox_half_size,  c_skybox_half_size),
	vec3(-c_skybox_half_size, -c_skybox_half_size,  c_skybox_half_size),
	vec3( c_skybox_half_size,  c_skybox_half_size, -c_skybox_half_size),
	vec3(-c_skybox_half_size,  c_skybox_half_size, -c_skybox_half_size),
	vec3( c_skybox_half_size, -c_skybox_half_size, -c_skybox_half_size),
	vec3(-c_skybox_half_size, -c_skybox_half_size, -c_skybox_half_size)
);

const int c_sky_indeces[12 * 3]= int[12 * 3]
(
	0, 1, 5,  0, 5, 4,
	0, 4, 6,  0, 6, 2,
	4, 5, 7,  4, 7, 6, // bottom
	0, 3, 1,  0, 2, 3, // top
	2, 7, 3,  2, 6, 7,
	1, 3, 7,  1, 7, 5
);

layout(binding= 0) uniform uniforms_block_variable
{
	mat4 view_matrix;
};

void main()
{
	vec3 pos= c_sky_vertices[c_sky_indeces[gl_VertexIndex]];
	gl_Position= view_matrix * vec4(pos, 1.0);
}
