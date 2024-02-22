#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/chunk_draw_info.glsl"
#include "inc/constants.glsl"

layout(binding= 0, std430) buffer chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

void main()
{
	// Just zero counter of new quads in this chunk.

	uint chunk_x= gl_GlobalInvocationID.x;
	uint chunk_y= gl_GlobalInvocationID.y;

	uint chunk_index= chunk_x + chunk_y * uint(c_chunk_matrix_size[0]);

	chunk_draw_info[chunk_index].new_num_quads= 0;
}