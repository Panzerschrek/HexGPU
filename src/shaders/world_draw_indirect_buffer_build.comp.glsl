#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

#include "inc/chunk_draw_info.glsl"
#include "inc/constants.glsl"
#include "inc/player_state.glsl"
#include "inc/vulkan_structs.glsl"

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 world_offset_chunks;
};

layout(binding= 0, std430) buffer readonly chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[];
};

layout(binding= 1, std430) buffer draw_indirect_buffer
{
	VkDrawIndexedIndirectCommand draw_commands[];
};

layout(binding= 2, std430) buffer water_draw_indirect_buffer
{
	VkDrawIndexedIndirectCommand water_draw_commands[];
};

layout(binding= 3, std430) buffer readonly player_state_buffer
{
	PlayerState player_state;
};

const uint c_indices_per_quad= 6;

void main()
{
	uint chunk_x= gl_GlobalInvocationID.x;
	uint chunk_y= gl_GlobalInvocationID.y;

	uint chunk_index= chunk_x + chunk_y * uint(world_size_chunks.x);

	ivec2 chunk_global_coord= ivec2(chunk_x, chunk_y) + world_offset_chunks;

	vec3 chunk_start_coord= vec3(
		float(chunk_global_coord.x) * (c_space_scale_x * float(c_chunk_width)),
		float(chunk_global_coord.y) * float(c_chunk_width),
		0.0);

	bool visible= true;
	for(int i= 0; i < 4; ++i)
	{
		// Approximate chunk as box.
		// TODO - add slight offsets - chunks are a little bigger than their box.
		int num_vertices_behind_the_plane= 0;
		for(int x= 0; x < 2; ++x)
		for(int y= 0; y < 2; ++y)
		for(int z= 0; z < 2; ++z)
		{
			vec3 vertex_offset= vec3(float(x * c_chunk_width) * c_space_scale_x, float(y * c_chunk_width), float(z * c_chunk_height));
			vec3 vertex_coord= chunk_start_coord + vertex_offset;
			if(dot(vec4(chunk_start_coord, 1.0), player_state.frustum_planes[i]) > 0.0)
				++num_vertices_behind_the_plane;
		}

		if(num_vertices_behind_the_plane == 8)
			visible= false;
	}

	{
		uint num_quads= chunk_draw_info[chunk_index].num_quads;
		uint first_quad= chunk_draw_info[chunk_index].first_quad;

		VkDrawIndexedIndirectCommand draw_command;
		draw_command.indexCount= visible ? (num_quads * c_indices_per_quad) : 0;
		draw_command.instanceCount= 1;
		draw_command.firstIndex= 0;
		draw_command.vertexOffset= int(first_quad) * 4;
		draw_command.firstInstance= 0;

		draw_commands[chunk_index]= draw_command;
	}

	{
		uint num_quads= chunk_draw_info[chunk_index].num_water_quads;
		uint first_quad= chunk_draw_info[chunk_index].first_water_quad;

		VkDrawIndexedIndirectCommand draw_command;
		draw_command.indexCount= num_quads * c_indices_per_quad;
		draw_command.instanceCount= 1;
		draw_command.firstIndex= 0;
		draw_command.vertexOffset= int(first_quad) * 4;
		draw_command.firstInstance= 0;

		water_draw_commands[chunk_index]= draw_command;
	}
}
