#version 450

// Use for sky mesh a pyramid with square base.

const float c_skybox_half_size= 512.0;
const float c_pyramid_height= 1024.0;
const float c_pyramid_depth= -512.0;

const vec3 c_sky_vertices[5]= vec3[5]
(
	vec3( c_skybox_half_size,  c_skybox_half_size, c_pyramid_depth),
	vec3(-c_skybox_half_size,  c_skybox_half_size, c_pyramid_depth),
	vec3( c_skybox_half_size, -c_skybox_half_size, c_pyramid_depth),
	vec3(-c_skybox_half_size, -c_skybox_half_size, c_pyramid_depth),
	vec3(0, 0, c_pyramid_height)
);

const int c_num_indices= 6 * 3;

const int c_sky_indeces[c_num_indices]= int[c_num_indices]
(
	// Pyramid sides
	4, 1, 0,
	4, 3, 1,
	4, 2, 3,
	4, 0, 2,
	// Pyramid base
	0, 1, 2,
	2, 1, 3
);

layout(binding= 0) uniform uniforms_block_variable
{
	mat4 view_matrix;
};

layout(location= 0) out vec3 f_view_vec;

void main()
{
	vec3 pos= c_sky_vertices[c_sky_indeces[gl_VertexIndex]];
	f_view_vec= pos;
	gl_Position= view_matrix * vec4(pos, 1.0);
}
