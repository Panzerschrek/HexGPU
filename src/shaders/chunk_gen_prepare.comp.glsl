#version 450

#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/chunk_gen_info.glsl"
#include "inc/structures.glsl"
#include "inc/trees_distribution.glsl"

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 chunk_position;
	ivec2 chunk_global_position;
	int seed;
};

layout(binding= 0, std430) buffer chunk_gen_info_buffer
{
	ChunkGenInfo chunk_gen_infos[];
};

layout(binding= 1, std430) buffer structure_descriptions_buffer
{
	StructureDescription structure_descriptions[];
};

layout(binding= 2, std430) buffer tree_map_buffer
{
	TreeMap tree_map;
};

void main()
{
	ChunkGenInfo info;
	info.num_structures= 0;

	for(int i= 0; i < 2; ++i)
	{
		uint8_t structure_id= uint8_t(i);

		u8vec4 structure_size= structure_descriptions[uint(structure_id)].size;

		int offset_x= (chunk_global_position.y & 7) + (i * 4);
		int offset_y= (chunk_global_position.x & 3) + (i * 8);

		info.structures[i].min= i8vec4(int8_t(offset_x), int8_t(offset_y), int8_t(50), int8_t(structure_id));
		// Adding extra for y 1 is important here to handle shifted columns.
		info.structures[i].max= i8vec4(int8_t(offset_x + int(structure_size.x)), int16_t(offset_y + int(structure_size.y) + 1), int8_t(50 + int(structure_size.z)), 0);

		++info.num_structures;
	}

	int chunk_index= chunk_position.x + chunk_position.y * world_size_chunks.x;
	chunk_gen_infos[chunk_index]= info;
}