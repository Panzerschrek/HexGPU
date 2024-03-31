#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/chunk_draw_info.glsl"

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 chunks_shift; // Must be always non-negative!
};

layout(binding= 0, std430) readonly buffer chunk_draw_info_input_buffer
{
	ChunkDrawInfo chunk_draw_info_input[];
};

layout(binding= 1, std430) writeonly buffer chunk_draw_info_output_buffer
{
	ChunkDrawInfo chunk_draw_info_output[];
};

void main()
{
	// Perform shift of chunk info from old position to new.
	// For chunks outside new area just perform wrapping.

	uint chunk_x= gl_GlobalInvocationID.x;
	uint chunk_y= gl_GlobalInvocationID.y;
	uint dst_chunk_index= chunk_x + chunk_y * uint(world_size_chunks.x);

	// % operation is well-defind as soon as both operands are non-negative.
	int src_chunk_x= (int(chunk_x) + chunks_shift.x) % world_size_chunks.x;
	int src_chunk_y= (int(chunk_y) + chunks_shift.y) % world_size_chunks.y;
	int src_chunk_index= src_chunk_x + src_chunk_y * world_size_chunks.x;

	chunk_draw_info_output[dst_chunk_index]= chunk_draw_info_input[src_chunk_index];
}
