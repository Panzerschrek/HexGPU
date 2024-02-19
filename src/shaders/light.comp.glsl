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

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(binding= 1, std430) buffer chunk_input_light_buffer
{
	uint8_t input_light[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(binding= 2, std430) buffer chunk_output_light_buffer
{
	uint8_t output_light[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

void main()
{
	int chunk_index= chunk_position[0] + chunk_position[1] * c_chunk_matrix_size[0];
	int chunk_data_offset= chunk_index * c_chunk_volume;

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

	uint8_t block_value= chunks_data[block_address];
	uint8_t optical_density= c_block_optical_density_table[int(block_value)];

	uint8_t own_light= c_block_own_light_table[int(block_value)];

	uint8_t result_light= uint8_t(0);
	if(optical_density == c_optical_density_solid)
	{
		// For solid blocks just set light to on light.
		// This allows to avoid reading adjusted light values and stops light propagation through walls.
		result_light= own_light;
	}
	else
	{
		// Non-solid block.

		// Read adjusted light and find maximum.
		int light_value_up= int(input_light[block_address_up]);
		int light_value_down= int(input_light[block_address_down]);
		int light_value_north= int(input_light[block_address_north]);
		int light_value_south= int(input_light[block_address_south]);
		int light_value_north_east= int(input_light[block_address_north_east]);
		int light_value_south_east= int(input_light[block_address_south_east]);
		int light_value_north_west= int(input_light[block_address_north_west]);
		int light_value_south_west= int(input_light[block_address_south_west]);

		int max_adjusted_light=
			max(
				max(
					max(light_value_up, light_value_down),
					max(light_value_north, light_value_south)),
				max(
					max(light_value_north_east, light_value_south_east),
					max(light_value_north_west, light_value_south_west)));

		// Result light for non-solid block is maximum adjusted block light minus one.
		// But it can't be less than block own light.
		result_light= uint8_t(max(int(own_light), max(max_adjusted_light - 1, 0)));
	}

	output_light[chunk_data_offset + ChunkBlockAddress(invocation.x, invocation.y, invocation.z)]= result_light;
}
