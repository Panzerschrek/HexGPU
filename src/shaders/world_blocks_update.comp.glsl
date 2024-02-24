#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"

layout(push_constant) uniform uniforms_block
{
	int chunk_position[2];
};

layout(binding= 0, std430) buffer chunks_data_input_buffer
{
	uint8_t chunks_input_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(binding= 1, std430) buffer chunks_data_output_buffer
{
	uint8_t chunks_output_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

void main()
{
	// Each thread of this shader transforms one block.
	// Input data buffer is used - for reading this block kind and its adjacent blocks.
	// Output databuffer is written only for this block.

	ivec3 invocation= ivec3(gl_GlobalInvocationID);

	int block_global_x= (chunk_position[0] << c_chunk_width_log2) + invocation.x;
	int block_global_y= (chunk_position[1] << c_chunk_width_log2) + invocation.y;
	int z= invocation.z;

	int side_y_base= block_global_y + ((block_global_x + 1) & 1);
	int east_x_clamped= min(block_global_x + 1, c_max_global_x);
	int west_x_clamped= max(block_global_x - 1, 0);

	// TODO - optimize this. Reuse calculations in the same chunk.
	int block_address= GetBlockFullAddress(ivec3(block_global_x, block_global_y, z));
	int block_address_up= GetBlockFullAddress(ivec3(block_global_x, block_global_y, min(z + 1, c_chunk_height - 1)));
	int block_address_down= GetBlockFullAddress(ivec3(block_global_x, block_global_y, max(z - 1, 0)));
	int block_address_north= GetBlockFullAddress(ivec3(block_global_x, min(block_global_y + 1, c_max_global_y), z));
	int block_address_south= GetBlockFullAddress(ivec3(block_global_x, max(block_global_y - 1, 0), z));
	int block_address_north_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(side_y_base - 0, c_max_global_y)), z));
	int block_address_south_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(side_y_base - 1, c_max_global_y)), z));
	int block_address_north_west= GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 0, c_max_global_y)), z));
	int block_address_south_west= GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 1, c_max_global_y)), z));

	uint8_t block_value= chunks_input_data[block_address];

	int chunk_index= chunk_position[0] + chunk_position[1] * c_chunk_matrix_size[0];
	int chunk_data_offset= chunk_index * c_chunk_volume;
	chunks_output_data[chunk_data_offset + ChunkBlockAddress(invocation.x, invocation.y, invocation.z)]= block_value;
}
