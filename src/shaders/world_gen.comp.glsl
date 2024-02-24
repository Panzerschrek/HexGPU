#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/noise.glsl"

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(push_constant) uniform uniforms_block
{
	int chunk_position[2];
};

int GetGroundLevel(int global_x, int global_y)
{
	// HACK. If not doing this, borders parallel to world X axis are to sharply.
	int global_y_corrected= global_y - (global_x & 1);

	int seed= 0; // TODO - provide seed value via an uniform.

	// Add several octaves of triangle-interpolated noise.
	int noise=
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed, 6)     ) +
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed, 5) >> 1) +
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed, 4) >> 2) +
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed, 3) >> 3);

	// TODO - scale result noise depending on current biome.
	int noise_scaled= noise >> 11;

	int base_ground_value= 2; // TODO - choose base value depending on current biome.

	return max(3, min(base_ground_value + noise_scaled, c_chunk_height - 2));
}

void main()
{
	int chunk_index= chunk_position[0] + chunk_position[1] * c_chunk_matrix_size[0];
	int chunk_data_offset= chunk_index * c_chunk_volume;

	int local_x= int(gl_GlobalInvocationID.x);
	int local_y= int(gl_GlobalInvocationID.y);
	int global_x= (chunk_position[0] << c_chunk_width_log2) + local_x;
	int global_y= (chunk_position[1] << c_chunk_width_log2) + local_y;

	int ground_z= GetGroundLevel(global_x, global_y);

	int column_offset= chunk_data_offset + ChunkBlockAddress(local_x, local_y, 0);

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
	chunks_data[column_offset + ground_z]= c_block_type_soil;

	// Fill remaining space with air.
	for(int z= ground_z + 1; z < c_chunk_height; ++z)
	{
		chunks_data[column_offset + z]= c_block_type_air;
	}

	// TODO - make water.
	// TODO - plant trees.
	// TODO - make caves.
}
