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
	ivec2 in_chunk_position;
	ivec2 out_chunk_position;
	uint current_tick;
	uint reserved;
};

layout(binding= 0, std430) buffer chunks_data_input_buffer
{
	uint8_t chunks_input_data[];
};

layout(binding= 1, std430) buffer chunks_data_output_buffer
{
	uint8_t chunks_output_data[];
};

layout(binding= 2, std430) buffer chunks_auxiliar_data_input_buffer
{
	uint8_t chunks_auxiliar_input_data[];
};

layout(binding= 3, std430) buffer chunks_auxiliar_data_output_buffer
{
	uint8_t chunks_auxiliar_output_data[];
};

// Returns pair of block type and auxiliar data.
u8vec2 TransformBlock(int block_x, int block_y, int z)
{
	ivec2 max_coord= GetMaxWorldCoord(world_size_chunks);

	int side_y_base= block_y + ((block_x + 1) & 1);
	int east_x_clamped= min(block_x + 1, max_coord.x);
	int west_x_clamped= max(block_x - 1, 0);

	int z_up_clamped= min(z + 1, c_chunk_height - 1);
	int z_down_clamped= max(z - 1, 0);

	int column_address= GetBlockFullAddress(ivec3(block_x, block_y, 0), world_size_chunks);

	int adjacent_columns[6]= int[6](
		// north
		GetBlockFullAddress(ivec3(block_x, min(block_y + 1, max_coord.y), 0), world_size_chunks),
		// south
		GetBlockFullAddress(ivec3(block_x, max(block_y - 1, 0), 0), world_size_chunks),
		// north-east
		GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(side_y_base - 0, max_coord.y)), 0), world_size_chunks),
		// south-east
		GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(side_y_base - 1, max_coord.y)), 0), world_size_chunks),
		// north-west
		GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 0, max_coord.y)), 0), world_size_chunks),
		// south-west
		GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 1, max_coord.y)), 0), world_size_chunks) );

	uint8_t block_type= chunks_input_data[column_address + z];

	// Assuming blocks on edges of the world inactive.
	// Some interactions for such blocks are disabled.
	bool is_in_active_area=
		block_x > 0 && block_x < max_coord.x &&
		block_y > 0 && block_y < max_coord.y;

	bool side_east_is_active= block_x + 1 < max_coord.x;
	bool side_west_is_active= block_x - 1 > 0;
	bool side_north_is_active= side_y_base > 0 && side_y_base < max_coord.y;
	bool side_south_is_active= side_y_base - 1 > 0 && side_y_base - 1 < max_coord.y;

	bool adjacent_column_is_in_active_area[6] = bool[6](
		// north
		block_y + 1 < max_coord.y,
		// south
		block_y - 1 > 0,
		// north-east
		side_east_is_active && side_north_is_active,
		// south-east
		side_east_is_active && side_south_is_active,
		// north-west
		side_west_is_active && side_north_is_active,
		// south-west
		side_west_is_active && side_south_is_active);

	// Allow blocks to fall down only each second tick.
	bool is_block_falling_tick= (current_tick & 1) == 0;
	// It's important to perform fall down logic and water side flow logic in separate ticks.
	// Doing so we prevent cases where water is suddenly replaced with sand, for example.
	bool is_water_side_flow_tick= (current_tick & 1) == 1;

	// Switch over block type.
	if(block_type == c_block_type_air)
	{
		if(is_block_falling_tick)
		{
			if( z < c_chunk_height - 1)
			{
				uint8_t block_above_type= chunks_input_data[column_address + z + 1];
				if(block_above_type == c_block_type_sand)
				{
					// If we have a sand block above, convert this block into sand.
					// This should match sand block logic.
					return u8vec2(c_block_type_sand, 0);
				}
				if(block_above_type == c_block_type_water)
				{
					// If we have a water block above, convert this block into water.
					// Use proper water level.
					// This should match water block logic.
					uint8_t water_level_above= chunks_auxiliar_input_data[column_address + z + 1];
					return u8vec2(c_block_type_water, water_level_above);
				}
			}
		}
		else if(is_water_side_flow_tick)
		{
			if(is_in_active_area)
			{
				// Check if water flows into this air block.
				// This should mirror water block logic!
				int flow_in= 0;

				for(int i= 0; i < 6; ++i) // For adjacent blocks.
				{
					if(!adjacent_column_is_in_active_area[i])
						continue; // Prevent flow from inactive area, since water level values aren't updated there.

					int adjacent_block_address= adjacent_columns[i] + z;

					uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
					if(adjacent_block_type == c_block_type_water)
					{
						bool adjacent_can_flow_down= false;
						if(z > 0)
						{
							int adjacent_block_below_address= adjacent_block_address - 1;
							uint8_t adjacent_block_below_type= chunks_input_data[adjacent_block_below_address];
							adjacent_can_flow_down=
								adjacent_block_below_type == c_block_type_air ||
								(adjacent_block_below_type == c_block_type_water &&  int(chunks_auxiliar_input_data[adjacent_block_below_address]) < c_max_water_level);
						}
						if(adjacent_can_flow_down)
							continue; // Prevent out flow if flow down is possilble.

						int adjacent_water_level= int(chunks_auxiliar_input_data[adjacent_block_address]);

						flow_in+= adjacent_water_level >> 3;
					}
				}

				if(flow_in != 0)
				{
					// Has some in water flow - convert into water.
					return u8vec2(c_block_type_water, flow_in);
				}
				else
					return u8vec2(c_block_type_air, 0);
			}
		}
	}
	else if(block_type == c_block_type_sand)
	{
		// If sand block has air below, it falls down and is replaced with air.
		// This should match air block logic.
		if(is_block_falling_tick &&
			z > 0 && chunks_input_data[column_address + z - 1] == c_block_type_air)
			return u8vec2(c_block_type_air, 0);
	}
	else if(block_type == c_block_type_water)
	{
		int water_level= int(chunks_auxiliar_input_data[column_address + z]);

		if(is_block_falling_tick)
		{
			// Process water flow down in block falling tick.
			// Caution!
			// Water flow-in for block below should be identical to flow-out for block above,
			// in order to preserve total amount of water!
			int flow_in= 0;
			int flow_out= 0;

			if(z > 0)
			{
				uint8_t block_below_type= chunks_input_data[column_address + z - 1];
				if(block_below_type == c_block_type_air)
				{
					// If we have air below - transfer all water.
					// This shoul match air block logic.
					flow_out= water_level;
				}
				else if(block_below_type == c_block_type_water)
				{
					int water_level_below= int(chunks_auxiliar_input_data[column_address + z - 1]);
					flow_out= min(water_level, c_max_water_level - water_level_below);
				}
			}
			if(z < c_chunk_height - 1)
			{
				uint8_t block_above_type= chunks_input_data[column_address + z + 1];
				if(block_above_type == c_block_type_water)
				{
					int water_level_above= int(chunks_auxiliar_input_data[column_address + z + 1]);
					flow_in= min(water_level_above, c_max_water_level - water_level);
				}
			}

			int new_water_level= water_level + flow_in - flow_out; // Should be in allowed range [0; c_max_water_level]
			if(new_water_level == 0)
			{
				// Nothing left in this block - convert it into air.
				return u8vec2(c_block_type_air, 0);
			}
			else
			{
				// Has some water left.
				return u8vec2(c_block_type_water, new_water_level);
			}
		}
		else if(is_water_side_flow_tick)
		{
			if(is_in_active_area)
			{
				// Perform water side flow.
				int flow_in= 0;
				int flow_out= 0;

				for(int i= 0; i < 6; ++i) // For adjacent blocks.
				{
					if(!adjacent_column_is_in_active_area[i])
						continue; // Prevent flow from inactive area, since water level values aren't updated there.

					int adjacent_block_address= adjacent_columns[i] + z;

					// Flow is 1/8 of difference on each tick.
					// Doing so we prevent water flow/underflow, since difference can't be more than water level or remaining capacity.
					// In the worst case with all blocks flowing in/out overflow isn't possible, because there are maximum 6 adjacent blocks.
					// It's possible to use 1/6, but 1/8 is better,
					// since it uses cheap bit shift instead of expensive integer division.

					uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
					if(adjacent_block_type == c_block_type_air)
					{
						flow_out+= water_level >> 3;
					}
					else if(adjacent_block_type == c_block_type_water)
					{
						int adjacent_water_level= int(chunks_auxiliar_input_data[adjacent_block_address]);

						int level_diff= water_level - adjacent_water_level;
						if(level_diff >= 8)
							flow_out+= level_diff >> 3;
						else if(level_diff <= -8)
						{
							// Prevent adjacent block out flow if flow down is possilble.
							bool adjacent_can_flow_down= false;
							if(z > 0)
							{
								int adjacent_block_below_address= adjacent_block_address - 1;
								uint8_t adjacent_block_below_type= chunks_input_data[adjacent_block_below_address];
								adjacent_can_flow_down=
									adjacent_block_below_type == c_block_type_air ||
									(adjacent_block_below_type == c_block_type_water && int(chunks_auxiliar_input_data[adjacent_block_below_address]) < c_max_water_level);
							}

							if(!adjacent_can_flow_down)
								flow_in+= (-level_diff) >> 3;
						}
						else
						{
							// Level diff is too low - perform no flow.
						}
					}
				}

				bool can_flow_down= false;
				if(z > 0)
				{
					int block_below_address= column_address + z - 1;
					uint8_t block_below_type= chunks_input_data[block_below_address];
					can_flow_down=
						block_below_type == c_block_type_air ||
						(block_below_type == c_block_type_water && int(chunks_auxiliar_input_data[block_below_address]) < c_max_water_level);
				}

				if(can_flow_down)
					flow_out= 0; // Prevent out flow if flow down is possilble.

				int new_water_level= water_level + flow_in - flow_out; // Should be in allowed range [0; c_max_water_level]
				// Assuming it's not possible to drain all water in side flow logic.
				return u8vec2(c_block_type_water, new_water_level);
			}
		}
	}
	else if(block_type == c_block_type_soil)
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
				return u8vec2(c_block_type_grass, 0);
		}
	}
	else if(block_type == c_block_type_grass)
	{
		// Grass disappears if is blocked from above.
		// TODO - take also light level into account.
		uint8_t block_above_type= chunks_input_data[column_address + z_up_clamped];
		if(!(block_above_type == c_block_type_air || block_above_type == c_block_type_foliage))
			return u8vec2(c_block_type_soil, 0);
	}

	// Common case when block type isn't chanhed.
	return u8vec2(block_type, chunks_auxiliar_input_data[column_address + z]);
}

void main()
{
	// Each thread of this shader transforms one block.
	// Input data buffer is used - for reading this block kind and its adjacent blocks.
	// Output databuffer is written only for this block.

	ivec3 invocation= ivec3(gl_GlobalInvocationID);

	int block_x= (in_chunk_position.x << c_chunk_width_log2) + invocation.x;
	int block_y= (in_chunk_position.y << c_chunk_width_log2) + invocation.y;
	int z= invocation.z;

	u8vec2 new_block_state= TransformBlock(block_x, block_y, z);

	// Write updated block.
	int chunk_index= out_chunk_position.x + out_chunk_position.y * world_size_chunks.x;
	int chunk_data_offset= chunk_index * c_chunk_volume;

	int address= chunk_data_offset + ChunkBlockAddress(invocation);

	chunks_output_data[address]= new_block_state.x;

	// TODO - avoid writing auxiliar data for blocks which don't use it?
	chunks_auxiliar_output_data[address]= new_block_state.y;
}
