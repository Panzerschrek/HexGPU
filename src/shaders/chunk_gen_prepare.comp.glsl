#version 450

#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/chunk_gen_info.glsl"

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

void main()
{
	ChunkGenInfo info;
	info.num_structures= 1;

	u8vec4 structure_size= u8vec4(3, 3, 5, 0);

	int offset_x= chunk_global_position.y & 7;
	int offset_y= chunk_global_position.x & 3;

	info.structures[0].min= i16vec4(int16_t(offset_x), int16_t(offset_y), int16_t(60), int16_t(0));
	// Adding extra for y 1 is important here to handle shifted columns.
	info.structures[0].max= i16vec4(int16_t(offset_x + int(structure_size.x)), int16_t(offset_y + int(structure_size.y) + 1), int16_t(60 + int(structure_size.z)), 0);

	int chunk_index= chunk_position.x + chunk_position.y * world_size_chunks.x;
	chunk_gen_infos[chunk_index]= info;
}
