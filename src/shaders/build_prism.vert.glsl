#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"

// Vertices of a hexogonal prism.
const vec3 c_vertices[12]= vec3[12]
(
	vec3(1.0, 0.0, 0.0),
	vec3(3.0, 0.0, 0.0),
	vec3(4.0, 1.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(3.0, 2.0, 0.0),
	vec3(1.0, 2.0, 0.0),

	vec3(1.0, 0.0, 1.0),
	vec3(3.0, 0.0, 1.0),
	vec3(4.0, 1.0, 1.0),
	vec3(0.0, 1.0, 1.0),
	vec3(3.0, 2.0, 1.0),
	vec3(1.0, 2.0, 1.0)
);

// Indices of a hexogonal prism triangles.
const int c_indices[60]= int[60]
(
	// up
	 8,  7,  6,
	 9,  8,  6,
	11, 10,  9,
	10,  8,  9,
	// down
	 0,  1,  2,
	 0,  2,  3,
	 3,  4,  5,
	 3,  2,  4,
	// north
	 5,  4, 10,
	 5, 10, 11,
	// south
	 1,  0,  6,
	 1,  6,  7,
	// north-east
	 4,  2,  8,
	 4,  8, 10,
	// south-east
	 2,  1,  7,
	 2,  7,  8,
	// north-west
	 3,  5,  9,
	 5, 11,  9,
	// south-west
	 0,  3,  9,
	 0,  9,  6
);

const uint8_t c_triangle_index_to_side[20]= uint8_t[20]
(
	c_direction_up, c_direction_up, c_direction_up, c_direction_up,
	c_direction_down, c_direction_down, c_direction_down, c_direction_down,
	c_direction_north, c_direction_north,
	c_direction_south, c_direction_south,
	c_direction_north_east, c_direction_north_east,
	c_direction_south_east, c_direction_south_east,
	c_direction_north_west, c_direction_north_west,
	c_direction_south_west, c_direction_south_west
);

layout(binding= 0) uniform uniforms_block_variable
{
	mat4 view_matrix;
	ivec4 build_pos; // w - direction
};

layout(location = 0) out float f_alpha;
layout(location = 1) out vec4 f_stripes;

void main()
{
	vec3 v= c_vertices[c_indices[gl_VertexIndex]];
	vec3 pos_corrected= v + vec3(build_pos.xyz) * vec3(3.0, 2.0, 1.0);
	pos_corrected.y+= float((build_pos.x ^ 1) & 1);
	gl_Position= view_matrix * vec4(pos_corrected, 1.0);

	int direction= c_triangle_index_to_side[gl_VertexIndex / 3];

	// Highlight active build prism side.
	f_alpha= direction == build_pos.w ? 0.8 : 0.2;

	f_stripes= vec4(2.0, 3.0, 3.0, 6.0) * vec4(v.x, v.y + (1.0 / 3.0) * v.x, v.y - (1.0 / 3.0) * v.x, v.z);
}
