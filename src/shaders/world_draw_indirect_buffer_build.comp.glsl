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

layout(binding= 4, std430) buffer fire_draw_indirect_buffer
{
	VkDrawIndexedIndirectCommand fire_draw_commands[];
};

layout(binding= 5, std430) buffer grass_draw_indirect_buffer
{
	VkDrawIndexedIndirectCommand grass_draw_commands[];
};

const uint c_indices_per_quad= 6;

bool IsChunkVisible(ivec2 chunk_global_coord)
{
	// Approximate chunk as box and check if this box is behind one of the clip planes.

	// Hexagonal grid extents a little bit outside chunk bounding box. Compensate this error.
	const vec2 chunk_border= vec2(0.5, 0.5);

	vec3 chunk_start_coord= vec3(
		float(chunk_global_coord.x) * (c_space_scale_x * float(c_chunk_width)) - chunk_border.x,
		float(chunk_global_coord.y) * float(c_chunk_width) - chunk_border.y,
		0.0);

	for(int i= 0; i < 5; ++i)
	{
		// Approximate chunk as box.
		int num_vertices_behind_plane= 0;
		for(int dx= 0; dx < 2; ++dx)
		for(int dy= 0; dy < 2; ++dy)
		for(int dz= 0; dz < 2; ++dz)
		{
			vec3 vertex_offset= vec3(
				float(dx) * (chunk_border.x * 2.0 + float(c_chunk_width) * c_space_scale_x),
				float(dy) * (chunk_border.y * 2.0 + float(c_chunk_width)),
				float(dz * c_chunk_height));
			vec3 vertex_coord= chunk_start_coord + vertex_offset;
			if(dot(vec4(vertex_coord, 1.0), player_state.frustum_planes[i]) > 0.0)
				++num_vertices_behind_plane;
		}

		if(num_vertices_behind_plane == 8)
			return false; // Totally clipped by this plane.
	}

	return true;
}

void main()
{
	uint chunk_x= gl_GlobalInvocationID.x;
	uint chunk_y= gl_GlobalInvocationID.y;

	uint chunk_index= chunk_x + chunk_y * uint(world_size_chunks.x);

	bool visible= IsChunkVisible(ivec2(chunk_x, chunk_y) + world_offset_chunks);

	{
		uint num_quads= visible ? chunk_draw_info[chunk_index].num_quads : 0;
		uint first_quad= visible ? chunk_draw_info[chunk_index].first_quad : 0;

		VkDrawIndexedIndirectCommand draw_command;
		draw_command.indexCount= num_quads * c_indices_per_quad;
		draw_command.instanceCount= 1;
		draw_command.firstIndex= 0;
		draw_command.vertexOffset= int(first_quad) * 4;
		draw_command.firstInstance= 0;

		draw_commands[chunk_index]= draw_command;
	}

	{
		uint num_quads= visible ? chunk_draw_info[chunk_index].num_water_quads : 0;
		uint first_quad= visible ? chunk_draw_info[chunk_index].first_water_quad : 0;

		VkDrawIndexedIndirectCommand draw_command;
		draw_command.indexCount= num_quads * c_indices_per_quad;
		draw_command.instanceCount= 1;
		draw_command.firstIndex= 0;
		draw_command.vertexOffset= int(first_quad) * 4;
		draw_command.firstInstance= 0;

		water_draw_commands[chunk_index]= draw_command;
	}

	{
		uint num_quads= visible ? chunk_draw_info[chunk_index].num_fire_quads : 0;
		uint first_quad= visible ? chunk_draw_info[chunk_index].first_fire_quad : 0;

		VkDrawIndexedIndirectCommand draw_command;
		draw_command.indexCount= num_quads * c_indices_per_quad;
		draw_command.instanceCount= 1;
		draw_command.firstIndex= 0;
		draw_command.vertexOffset= int(first_quad) * 4;
		draw_command.firstInstance= 0;

		fire_draw_commands[chunk_index]= draw_command;
	}

	{
		uint num_quads= visible ? chunk_draw_info[chunk_index].num_grass_quads : 0;
		uint first_quad= visible ? chunk_draw_info[chunk_index].first_grass_quad : 0;

		VkDrawIndexedIndirectCommand draw_command;
		draw_command.indexCount= num_quads * c_indices_per_quad;
		draw_command.instanceCount= 1;
		draw_command.firstIndex= 0;
		draw_command.vertexOffset= int(first_quad) * 4;
		draw_command.firstInstance= 0;

		grass_draw_commands[chunk_index]= draw_command;
	}
}
