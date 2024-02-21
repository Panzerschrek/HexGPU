#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/chunk_draw_info.glsl"
#include "inc/constants.glsl"
#include "inc/vulkan_structs.glsl"

layout(binding= 0, std430) buffer chunk_draw_info_buffer
{

	ChunkDrawInfo chunk_draw_info[c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(binding= 1, std430) buffer draw_indirect_buffer
{

	VkDrawIndexedIndirectCommand draw_commands[c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

const uint c_indices_per_quad= 6;

const uint c_max_quads_per_chunk= 65536 / 4;

void main()
{
	uint chunk_x= gl_GlobalInvocationID.x;
	uint chunk_y= gl_GlobalInvocationID.y;

	uint chunk_index= chunk_x + chunk_y * uint(c_chunk_matrix_size[0]);

	uint num_quads= chunk_draw_info[chunk_index].num_quads;

	// For now use same capacity for quads of all chunks.
	// TODO - allocate memory for chunk quads on per-chunk basis, read here offset to allocated memory.
	uint quads_offset= c_max_quads_per_chunk * chunk_index;

	VkDrawIndexedIndirectCommand draw_command;
	draw_command.indexCount= num_quads * c_indices_per_quad;
	draw_command.instanceCount= 1;
	draw_command.firstIndex= 0;
	draw_command.vertexOffset= int(chunk_index * c_max_quads_per_chunk) * 4;
	draw_command.firstInstance= 0;

	draw_commands[chunk_index]= draw_command;
}
