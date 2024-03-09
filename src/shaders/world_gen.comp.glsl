#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/chunk_gen_info.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/noise.glsl"
#include "inc/structures.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 8, local_size_y = 16, local_size_z= 1) in;

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 chunk_position;
	ivec2 chunk_global_position;
	int seed;
};

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};

layout(binding= 1, std430) buffer chunk_gen_info_buffer
{
	ChunkGenInfo chunk_gen_infos[];
};

layout(binding= 2, std430) buffer structure_descriptions_buffer
{
	StructureDescription structure_descriptions[];
};

layout(binding= 3, std430) buffer structures_data_buffer
{
	uint8_t structures_data[];
};

int GetGroundLevel(int global_x, int global_y)
{
	// HACK. If not doing this, borders parallel to world X axis are to sharply.
	int global_y_corrected= global_y - (global_x & 1);

	// Add several octaves of triangle-interpolated noise.
	// Use seed with offset to avoid fractal noise apperiance at world center (0, 0).
	int noise=
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed + 0, 6)     ) +
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed + 1, 5) >> 1) +
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed + 2, 4) >> 2) +
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed + 3, 3) >> 3);

	// TODO - scale result noise depending on current biome.
	int noise_scaled= noise >> 11;

	int base_ground_value= 2; // TODO - choose base value depending on current biome.

	return max(3, min(base_ground_value + noise_scaled, c_chunk_height - 2));
}

void main()
{
	int chunk_index= chunk_position.x + chunk_position.y * world_size_chunks.x;
	int chunk_data_offset= chunk_index * c_chunk_volume;

	int local_x= int(gl_GlobalInvocationID.x);
	int local_y= int(gl_GlobalInvocationID.y);
	int global_x= (chunk_global_position.x << c_chunk_width_log2) + local_x;
	int global_y= (chunk_global_position.y << c_chunk_width_log2) + local_y;

	int ground_z= GetGroundLevel(global_x, global_y);

	int column_offset= chunk_data_offset + ChunkBlockAddress(ivec3(local_x, local_y, 0));

	// Zero level - place single block of special type.
	chunks_data[column_offset]= c_block_type_spherical_block;

	// Place stone up to the ground layer.
	for(int z= 1; z < ground_z - 2; ++z)
	{
		chunks_data[column_offset + z]= c_block_type_stone;
	}

	// TODO - make soil layer variable-height.
	chunks_data[column_offset + ground_z - 2]= c_block_type_soil;
	chunks_data[column_offset + ground_z - 1]= c_block_type_soil;
	chunks_data[column_offset + ground_z]= c_block_type_grass;

	// Fill remaining space with air.
	for(int z= ground_z + 1; z < c_chunk_height; ++z)
	{
		chunks_data[column_offset + z]= c_block_type_air;
	}

	uint num_structures= chunk_gen_infos[chunk_index].num_structures;
	for(uint chunk_structure_index= 0; chunk_structure_index < num_structures; ++chunk_structure_index)
	{
		ChunkStructureDescription chunk_structure_description= chunk_gen_infos[chunk_index].structures[chunk_structure_index];
		uint strukcture_kind_index= uint(chunk_structure_description.min.w);
		StructureDescription structure_description= structure_descriptions[strukcture_kind_index];
		if( local_x >= chunk_structure_description.min.x && local_x < chunk_structure_description.max.x &&
			local_y >= chunk_structure_description.min.y && local_y < chunk_structure_description.max.y)
		{
			int rel_x= local_x - chunk_structure_description.min.x;
			int rel_y= local_y - chunk_structure_description.min.y;
			if((chunk_structure_description.min.x & 1) != 0 && (rel_x & 1) == 0)
			{
				--rel_y;
			}
			if(rel_y >= 0 && rel_y < int(structure_description.size.y))
			{
				int column_data_offset=
					int(structure_description.data_offset) +
					rel_y * structure_description.size.z + rel_x * (structure_description.size.z * structure_description.size.y);
				for(int z= chunk_structure_description.min.z; z < chunk_structure_description.max.z; ++z)
				{
					int rel_z= z - chunk_structure_description.min.z;
					int structure_block_offset= rel_z + rel_y * structure_description.size.z + rel_x * (structure_description.size.z * structure_description.size.y);

					chunks_data[column_offset + z]= structures_data[column_data_offset + rel_z];
				}
			}
		}
	}

	// TODO - make water.
	// TODO - plant trees.
	// TODO - make caves.
}
