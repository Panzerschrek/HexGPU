#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/chunk_draw_info.glsl"
#include "inc/constants.glsl"

layout(push_constant) uniform uniforms_block
{
	int world_size_chunks[2];
};

layout(binding= 0, std430) buffer chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[];
};

void main()
{
	// Just zero counter of new quads in this chunk.

	uint chunk_x= gl_GlobalInvocationID.x;
	uint chunk_y= gl_GlobalInvocationID.y;

	uint chunk_index= chunk_x + chunk_y * uint(world_size_chunks[0]);

	chunk_draw_info[chunk_index].new_num_quads= 0;
}
