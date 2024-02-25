#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"

// maxComputeWorkGroupInvocations is at least 128.
layout(local_size_x= 4, local_size_y = 4, local_size_z= 8) in;

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

uint8_t TransformBlock(int block_global_x, int block_global_y, int z)
{
	int side_y_base= block_global_y + ((block_global_x + 1) & 1);
	int east_x_clamped= min(block_global_x + 1, c_max_global_x);
	int west_x_clamped= max(block_global_x - 1, 0);

	int z_up_clamped= min(z + 1, c_chunk_height - 1);
	int z_down_clamped= max(z - 1, 0);

	int column_address= GetBlockFullAddress(ivec3(block_global_x, block_global_y, 0));

	int adjacent_columns[6]= int[6](
		// north
		GetBlockFullAddress(ivec3(block_global_x, min(block_global_y + 1, c_max_global_y), 0)),
		// south
		GetBlockFullAddress(ivec3(block_global_x, max(block_global_y - 1, 0), 0)),
		// north-east
		GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(side_y_base - 0, c_max_global_y)), 0)),
		// south-east
		GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(side_y_base - 1, c_max_global_y)), 0)),
		// north-west
		GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 0, c_max_global_y)), 0)),
		// south-west
		GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 1, c_max_global_y)), 0)) );

	uint8_t block_type= chunks_input_data[column_address + z];

	// Switch over block type.
	if(block_type == c_block_type_soil)
	{
		// Soil may be converted into grass, if there is a grass block nearby.

		// TODO - check if has enough light.

		// TODO - perform grass propagation checks not each tick, but only sometimes randomly.

		int num_adjacent_grass_blocks= 0;

		// Do not plant grass at top of the world and at very bottom.
		if( z >= 1 &&
			z < c_chunk_height - 3 &&
			// Plant only if has air abowe.
			chunks_input_data[column_address + z_up_clamped] == c_block_type_air)
		{
			// Check adjacent blocks. If has grass - convert into grass.
			for(int i= 0; i < 6; ++i)
			{
				uint8_t z_minus_one_block_type= chunks_input_data[adjacent_columns[i] + z - 1];
				uint8_t z_plus_zero_block_type= chunks_input_data[adjacent_columns[i] + z + 0];
				uint8_t z_plus_one_block_type = chunks_input_data[adjacent_columns[i] + z + 1];
				uint8_t z_plus_two_block_type = chunks_input_data[adjacent_columns[i] + z + 2];

				if( z_minus_one_block_type == c_block_type_grass &&
					z_plus_zero_block_type == c_block_type_air &&
					z_plus_one_block_type  == c_block_type_air)
				{
					// Block below is grass and has enough air.
					++num_adjacent_grass_blocks;
				}
				if( z_plus_zero_block_type == c_block_type_grass &&
					z_plus_one_block_type  == c_block_type_air)
				{
					// Block nearby is grass and has enough air.
					++num_adjacent_grass_blocks;
				}
				if( z_plus_one_block_type == c_block_type_grass &&
					z_plus_two_block_type == c_block_type_air &&
					chunks_input_data[column_address + z + 2] == c_block_type_air)
				{
					// Block above is grass and has enough air.
					++num_adjacent_grass_blocks;
				}
			}

			// TODO - perform convertion into grass randomly with chance proportional to number of grass blocks nearby.
			if(num_adjacent_grass_blocks > 0)
				return c_block_type_grass;
		}
	}
	else if(block_type == c_block_type_grass)
	{
		// Grass disappears if is blocked from above.
		// TODO - take also light level into account.
		uint8_t block_above_type= chunks_input_data[column_address + z_up_clamped];
		if(!(block_above_type == c_block_type_air || block_above_type == c_block_type_foliage))
			return c_block_type_soil;
	}

	// Common case when block type isn't chanhed.
	return block_type;
}

void main()
{
	// Each thread of this shader transforms one block.
	// Input data buffer is used - for reading this block kind and its adjacent blocks.
	// Output databuffer is written only for this block.

	ivec3 invocation= ivec3(gl_GlobalInvocationID);

	int block_global_x= (chunk_position[0] << c_chunk_width_log2) + invocation.x;
	int block_global_y= (chunk_position[1] << c_chunk_width_log2) + invocation.y;
	int z= invocation.z;

	uint8_t new_block_type= TransformBlock(block_global_x, block_global_y, z);

	// Write updated block.
	int chunk_index= chunk_position[0] + chunk_position[1] * c_chunk_matrix_size[0];
	int chunk_data_offset= chunk_index * c_chunk_volume;
	chunks_output_data[chunk_data_offset + ChunkBlockAddress(invocation.x, invocation.y, invocation.z)]= new_block_type;
}
