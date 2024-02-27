#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/chunk_draw_info.glsl"
#include "inc/hex_funcs.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 4, local_size_y = 4, local_size_z= 8) in;

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};

layout(binding= 1, std430) buffer chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[];
};

layout(push_constant) uniform uniforms_block
{
	int chunk_position[2];
};

void main()
{
	// Calculate only number of result quads.
	// This code must mutch code in geometry generation code!

	int chunk_index= chunk_position[0] + chunk_position[1] * c_chunk_matrix_size[0];

	uvec3 invocation= gl_GlobalInvocationID;

	int block_global_x= (chunk_position[0] << c_chunk_width_log2) + int(invocation.x);
	int block_global_y= (chunk_position[1] << c_chunk_width_log2) + int(invocation.y);
	int z= int(invocation.z);

	int east_x_clamped= min(block_global_x + 1, c_max_global_x);
	int east_y_base= block_global_y + ((block_global_x + 1) & 1);

	// TODO - optimize this. Reuse calculations in the same chunk.
	int block_address= GetBlockFullAddress(ivec3(block_global_x, block_global_y, z));
	int block_address_up= GetBlockFullAddress(ivec3(block_global_x, block_global_y, min(z + 1, c_chunk_height - 1)));
	int block_address_north= GetBlockFullAddress(ivec3(block_global_x, min(block_global_y + 1, c_max_global_y), z));
	int block_address_north_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(east_y_base - 0, c_max_global_y)), z));
	int block_address_south_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(east_y_base - 1, c_max_global_y)), z));

	uint8_t block_value= chunks_data[block_address];
	uint8_t block_value_up= chunks_data[block_address_up];
	uint8_t block_value_north= chunks_data[block_address_north];
	uint8_t block_value_north_east= chunks_data[block_address_north_east];
	uint8_t block_value_south_east= chunks_data[block_address_south_east];

	uint8_t optical_density= c_block_optical_density_table[int(block_value)];
	uint8_t optical_density_up= c_block_optical_density_table[int(block_value_up)];
	uint8_t optical_density_north= c_block_optical_density_table[int(block_value_north)];
	uint8_t optical_density_north_east= c_block_optical_density_table[int(block_value_north_east)];
	uint8_t optical_density_south_east= c_block_optical_density_table[int(block_value_south_east)];

	if(optical_density != optical_density_up)
	{
		// Add two hexagon quads.
		atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 2);
	}

	if(optical_density != optical_density_north)
	{
		// Add north quad.
		atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
	}

	if(optical_density != optical_density_north_east)
	{
		// Add north-east quad.
		atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
	}

	if(optical_density != optical_density_south_east)
	{
		// Add south-east quad.
		atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
	}
}
