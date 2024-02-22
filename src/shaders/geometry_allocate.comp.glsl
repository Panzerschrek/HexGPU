#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/allocator.glsl"
#include "inc/chunk_draw_info.glsl"
#include "inc/constants.glsl"

layout(binding= 0, std430) buffer chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

const int c_max_quads_per_chunk= 65536 / 4;

void main()
{
	// Perform updates for all chunks in single thread.
	for(int chunk_y= 0; chunk_y < c_chunk_matrix_size[1]; ++chunk_y)
	for(int chunk_x= 0; chunk_x < c_chunk_matrix_size[0]; ++chunk_x)
	{
		int chunk_index= chunk_x + chunk_y * c_chunk_matrix_size[0];
		chunk_draw_info[chunk_index].num_quads= 0;

		// For now use same capacity for quads of all chunks.
		// TODO - allocate memory for chunk quads on per-chunk basis, read here offset to allocated memory.
		chunk_draw_info[chunk_index].first_quad= uint(c_max_quads_per_chunk * chunk_index);
	}
}
