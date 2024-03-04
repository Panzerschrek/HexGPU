#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/chunk_draw_info.glsl"
#include "inc/vulkan_structs.glsl"

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
};

layout(binding= 0, std430) buffer chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[];
};

layout(binding= 1, std430) buffer draw_indirect_buffer
{
	VkDrawIndexedIndirectCommand draw_commands[];
};

const uint c_indices_per_quad= 6;

void main()
{
	uint chunk_x= gl_GlobalInvocationID.x;
	uint chunk_y= gl_GlobalInvocationID.y;

	uint chunk_index= chunk_x + chunk_y * uint(world_size_chunks.x);

	uint num_quads= chunk_draw_info[chunk_index].num_quads;
	uint first_quad= chunk_draw_info[chunk_index].first_quad;

	VkDrawIndexedIndirectCommand draw_command;
	draw_command.indexCount= num_quads * c_indices_per_quad;
	draw_command.instanceCount= 1;
	draw_command.firstIndex= 0;
	draw_command.vertexOffset= int(first_quad) * 4;
	draw_command.firstInstance= 0;

	draw_commands[chunk_index]= draw_command;
}
