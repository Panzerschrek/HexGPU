#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 4, local_size_y = 4, local_size_z= 8) in;

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 chunk_position;
	ivec2 chunk_global_position;
};

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};

layout(binding= 1, std430) buffer chunk_input_light_buffer
{
	uint8_t input_light[];
};

layout(binding= 2, std430) buffer chunk_output_light_buffer
{
	uint8_t output_light[];
};

void main()
{
	// Each thread of this shader calculates light for one block.
	// Input light buffer is used - for light fetches of adjacent blocks.
	// Output light buffer is written only for this block.

	ivec3 invocation= ivec3(gl_GlobalInvocationID);

	int block_x= (chunk_position.x << c_chunk_width_log2) + invocation.x;
	int block_y= (chunk_position.y << c_chunk_width_log2) + invocation.y;
	int z= invocation.z;

	ivec2 max_world_coord= GetMaxWorldCoord(world_size_chunks);

	int side_y_base= block_y + ((block_x + 1) & 1);
	int east_x_clamped= min(block_x + 1, max_world_coord.x);
	int west_x_clamped= max(block_x - 1, 0);

	// TODO - optimize this. Reuse calculations in the same chunk.
	int block_address= GetBlockFullAddress(ivec3(block_x, block_y, z), world_size_chunks);
	int block_address_up= GetBlockFullAddress(ivec3(block_x, block_y, min(z + 1, c_chunk_height - 1)), world_size_chunks);
	int block_address_down= GetBlockFullAddress(ivec3(block_x, block_y, max(z - 1, 0)), world_size_chunks);
	int block_address_north= GetBlockFullAddress(ivec3(block_x, min(block_y + 1, max_world_coord.y), z), world_size_chunks);
	int block_address_south= GetBlockFullAddress(ivec3(block_x, max(block_y - 1, 0), z), world_size_chunks);
	int block_address_north_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(side_y_base - 0, max_world_coord.y)), z), world_size_chunks);
	int block_address_south_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(side_y_base - 1, max_world_coord.y)), z), world_size_chunks);
	int block_address_north_west= GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 0, max_world_coord.y)), z), world_size_chunks);
	int block_address_south_west= GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 1, max_world_coord.y)), z), world_size_chunks);

	uint8_t block_value= chunks_data[block_address];
	uint8_t optical_density= c_block_optical_density_table[int(block_value)];

	uint8_t own_fire_light= c_block_own_light_table[int(block_value)];

	uint8_t result_light= uint8_t(0);
	if(optical_density == c_optical_density_solid)
	{
		// For solid blocks just set its light to own light (determined by block type).
		// This allows to avoid reading adjacent light values and stops light propagation through walls.
		result_light= own_fire_light;
	}
	else
	{
		// Non-solid block.

		// Read adjacent light and find maximum.
		int light_value_up= int(input_light[block_address_up]);
		int light_value_down= int(input_light[block_address_down]);
		int light_value_north= int(input_light[block_address_north]);
		int light_value_south= int(input_light[block_address_south]);
		int light_value_north_east= int(input_light[block_address_north_east]);
		int light_value_south_east= int(input_light[block_address_south_east]);
		int light_value_north_west= int(input_light[block_address_north_west]);
		int light_value_south_west= int(input_light[block_address_south_west]);

		int max_adjacent_fire_light=
			max(
				max(
					max(light_value_up    & c_fire_light_mask, light_value_down  & c_fire_light_mask),
					max(light_value_north & c_fire_light_mask, light_value_south & c_fire_light_mask)),
				max(
					max(light_value_north_east & c_fire_light_mask, light_value_south_east & c_fire_light_mask),
					max(light_value_north_west & c_fire_light_mask, light_value_south_west & c_fire_light_mask)));

		// Result light for non-solid block is maximum adjacent block light minus one.
		// But it can't be less than block own light.
		int result_fire_light= max(int(own_fire_light), max(max_adjacent_fire_light - 1, 0));

		int result_sky_light= 0;
		if(z == c_chunk_height - 1)
		{
			// Highest blocks recieve maximum sky light.
			result_sky_light= c_max_sky_light << c_sky_light_shift;
		}
		else if((light_value_up >> c_sky_light_shift) == c_max_sky_light && block_value == c_block_type_air)
		{
			// Sky light with highest value propagates down without losses, but only for air.
			result_sky_light= c_max_sky_light << c_sky_light_shift;
		}
		else
		{
			// Propagate sky light with less than maximum intensity like fire light.
			int max_adjacent_sky_light=
				max(
					max(
						max(light_value_up    >> c_sky_light_shift, light_value_down  >> c_sky_light_shift),
						max(light_value_north >> c_sky_light_shift, light_value_south >> c_sky_light_shift)),
					max(
						max(light_value_north_east >> c_sky_light_shift, light_value_south_east >> c_sky_light_shift),
						max(light_value_north_west >> c_sky_light_shift, light_value_south_west >> c_sky_light_shift)));

			result_sky_light= max(max_adjacent_sky_light - 1, 0) << c_sky_light_shift;
		}

		result_light= uint8_t(result_fire_light | result_sky_light);
	}

	int chunk_index= chunk_position.x + chunk_position.y * world_size_chunks.x;
	int chunk_data_offset= chunk_index * c_chunk_volume;
	output_light[chunk_data_offset + ChunkBlockAddress(invocation)]= result_light;
}
