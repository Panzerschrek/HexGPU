#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/noise.glsl"
#include "inc/world_global_state.glsl"

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

layout(binding= 0, std430) readonly buffer chunks_data_input_buffer
{
	uint8_t chunks_input_data[];
};

layout(binding= 1, std430) writeonly buffer chunks_data_output_buffer
{
	uint8_t chunks_output_data[];
};

layout(binding= 2, std430) readonly buffer chunks_auxiliar_data_input_buffer
{
	uint8_t chunks_auxiliar_input_data[];
};

layout(binding= 3, std430) writeonly buffer chunks_auxiliar_data_output_buffer
{
	uint8_t chunks_auxiliar_output_data[];
};

layout(binding= 4, std430) readonly buffer chunks_light_data_buffer
{
	uint8_t light_data[];
};

layout(binding= 5, std430) readonly buffer world_global_state_buffer
{
	WorldGlobalState world_global_state;
};

const int c_min_wetness_for_grass_to_exist= 3;

bool CanPlaceSnowOnThisBlock(uint8_t block_type)
{
	return
		block_type != c_block_type_air &&
		block_type != c_block_type_fire &&
		block_type != c_block_type_water &&
		block_type != c_block_type_snow;
}

const int c_max_fire_light_for_snow_to_exist= 7;

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

	// TODO - use global coord for rand.
	int block_rand= hex_Noise3(block_x, block_y, z, int(current_tick));

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

		int flow_in= 0;
		int total_fire_power_nearby= 0;
		int total_flammability_nearby= 0;

		for(int i= 0; i < 6; ++i) // For adjacent blocks.
		{
			int adjacent_block_address= adjacent_columns[i] + z;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];

			total_flammability_nearby+= int(c_block_flammability_table[uint(adjacent_block_type)]);

			if(adjacent_block_type == c_block_type_water)
			{
				// Check if water flows into this air block.
				// This should mirror water block logic!
				if(adjacent_column_is_in_active_area[i])
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
			if(adjacent_block_type == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address]);
		}
		if(z < c_chunk_height - 1)
		{
			int adjacent_block_address= column_address + z + 1;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			total_flammability_nearby+= int(c_block_flammability_table[uint(adjacent_block_type)]);

			if(adjacent_block_type == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address]);
		}
		if(z > 0)
		{
			int adjacent_block_address= column_address + z - 1;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			total_flammability_nearby+= int(c_block_flammability_table[uint(adjacent_block_type)]);

			if(adjacent_block_type == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address]);
		}

		// Perform water flow in water flow tick. This should have highest priority!
		if(is_water_side_flow_tick)
		{
			if(flow_in != 0)
			{
				// Has some in water flow - convert into water.
				return u8vec2(c_block_type_water, flow_in);
			}
			else
				return u8vec2(c_block_type_air, 0);
		}

		// Try to convert into fire.
		if(total_flammability_nearby > 0)
		{
			if((block_rand & 15) == 0)
			{
				if(total_fire_power_nearby >= c_min_fire_power_for_fire_to_spread)
				{
					// There are fire blocks nearby. Immideately convert into fire.
					return u8vec2(c_block_type_fire, c_initial_fire_power);
				}

				// Check if there are fire blocks nearby below/above, which are accessible via air block.

				bool block_below_is_air=
					z > 0 && chunks_input_data[column_address + z - 1] == c_block_type_air;
				bool block_above_is_air=
					z < c_chunk_height - 1 && chunks_input_data[column_address + z + 1] == c_block_type_air;

				for(int i= 0; i < 6; ++i) // For adjacent blocks.
				{
					int adjacent_block_address= adjacent_columns[i] + z;
					bool adjacent_block_is_air= chunks_input_data[adjacent_block_address] == c_block_type_air;

					if((adjacent_block_is_air || block_below_is_air) &&
						z > 0 && chunks_input_data[adjacent_block_address - 1] == c_block_type_fire)
						total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address - 1]);

					if((adjacent_block_is_air || block_above_is_air) &&
						z < c_chunk_height - 1 && chunks_input_data[adjacent_block_address + 1] == c_block_type_fire)
						total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address + 1]);
				}

				if(total_fire_power_nearby >= c_min_fire_power_for_fire_to_spread)
					return u8vec2(c_block_type_fire, c_initial_fire_power);
			}
		}

		// Try to convert into snow.
		if(z >= world_global_state.snow_z_level &&
			(block_rand & 15) == 0 &&
			CanPlaceSnowOnThisBlock(chunks_input_data[column_address + z - 1]))
		{
			// Snow can exist only direct under sky.
			int light_packed= light_data[column_address + z_up_clamped];
			int sky_light= light_packed >> c_sky_light_shift;
			int fire_light= light_packed & c_fire_light_mask;

			if(sky_light == c_max_sky_light && fire_light <= c_max_fire_light_for_snow_to_exist)
				return u8vec2(c_block_type_snow, 0);
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
				uint8_t block_below_type= c_block_type_spherical_block;
				if(z > 0)
				{
					int block_below_address= column_address + z - 1;
					block_below_type= chunks_input_data[block_below_address];
					can_flow_down=
						block_below_type == c_block_type_air ||
						(block_below_type == c_block_type_water && int(chunks_auxiliar_input_data[block_below_address]) < c_max_water_level);
				}

				if(can_flow_down)
					flow_out= 0; // Prevent out flow if flow down is possilble.

				if(water_level < 8 && flow_in == 0 && flow_out == 0 &&
					block_below_type != c_block_type_air &&
					block_below_type != c_block_type_water)
				{
					// Randomly evaporate this block if water level doesn't allow its content to flow sideways and it can't flow down.
					if((block_rand & 15) == 0)
						return u8vec2(c_block_type_air, 0);
				}

				int new_water_level= water_level + flow_in - flow_out; // Should be in allowed range [0; c_max_water_level]
				// Assuming it's not possible to drain all water in side flow logic.
				return u8vec2(c_block_type_water, new_water_level);
			}
		}
	}
	else if(block_type == c_block_type_soil)
	{
		// Soil may be converted into grass, if there is a grass block nearby.
		// Perform such conversion check randomly.

		if((block_rand & 15) == 0)
		{
			int num_adjacent_grass_blocks= 0;

			// Do not plant grass at top of the world and at very bottom.
			if( z >= 1 &&
				z < c_chunk_height - 3 &&
				// Plant only if has air abowe.
				chunks_input_data[column_address + z_up_clamped] == c_block_type_air)
			{
				// Require light to graw.
				int light_packed= light_data[column_address + z_up_clamped];
				int fire_light= light_packed & c_fire_light_mask;
				int sky_light= (light_packed >> c_sky_light_shift) & world_global_state.sky_light_mask;
				int total_light= fire_light + sky_light;
				const int c_min_light_to_graw= 2;
				if(total_light >= c_min_light_to_graw)
				{
					// Assuming areas with large amount of sky light are located under the sky and thus rain affects such areas.
					int wetness= (light_packed >> c_sky_light_shift) & world_global_state.sky_light_based_wetness_mask;

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

						if(z_plus_zero_block_type == c_block_type_water || z_minus_one_block_type == c_block_type_water)
							wetness= c_min_wetness_for_grass_to_exist;
					}

					// Conversion chance depends on number of grass blocks nearby.
					if(wetness >= c_min_wetness_for_grass_to_exist &&
						num_adjacent_grass_blocks * block_rand >= 65536 / 2)
						return u8vec2(c_block_type_grass, 0);
				}
			}
		}
	}
	else if(block_type == c_block_type_grass)
	{
		// Grass disappears if is blocked from above.
		uint8_t block_above_type= chunks_input_data[column_address + z_up_clamped];
		if(!(
			block_above_type == c_block_type_air ||
			block_above_type == c_block_type_snow ||
			block_above_type == c_block_type_foliage ||
			block_above_type == c_block_type_fire))
			return u8vec2(c_block_type_soil, 0);

		// Grass requires some level of wetness to exist.

		// Assuming areas with large amount of sky light are located under the sky and thus rain affects such areas.
		int wetness= (light_data[column_address + z_up_clamped] >> c_sky_light_shift) & world_global_state.sky_light_based_wetness_mask;

		if(wetness < c_min_wetness_for_grass_to_exist)
		{
			// Check if has water nearby.
			for(int i= 0; i < 6; ++i)
			{
				int adjacent_block_address= adjacent_columns[i] + z;
				if(chunks_input_data[adjacent_block_address] == c_block_type_water)
					wetness= c_min_wetness_for_grass_to_exist;
				if(z > 0 && chunks_input_data[adjacent_block_address - 1] == c_block_type_water)
					wetness= c_min_wetness_for_grass_to_exist;
			}

			if(wetness < c_min_wetness_for_grass_to_exist)
			{
				// If wentess is not enough - randomly make grass yellow.
				if((block_rand & 15) == 0)
					return u8vec2(c_block_type_grass_yellow, 0);
			}
		}
	}
	else if(block_type == c_block_type_grass_yellow)
	{
		// Grass disappears if is blocked from above.
		uint8_t block_above_type= chunks_input_data[column_address + z_up_clamped];
		if(!(
			block_above_type == c_block_type_air ||
			block_above_type == c_block_type_snow ||
			block_above_type == c_block_type_foliage ||
			block_above_type == c_block_type_fire))
			return u8vec2(c_block_type_soil, 0);

		// Assuming areas with large amount of sky light are located under the sky and thus rain affects such areas.
		int wetness= (light_data[column_address + z_up_clamped] >> c_sky_light_shift) & world_global_state.sky_light_based_wetness_mask;

		// Check if has water or fire nearby.
		int total_fire_power_nearby= 0;
		for(int i= 0; i < 6; ++i)
		{
			int adjacent_block_address= adjacent_columns[i] + z;

			if(chunks_input_data[adjacent_block_address] == c_block_type_water)
				wetness= c_min_wetness_for_grass_to_exist;
			if(z > 0 && chunks_input_data[adjacent_block_address - 1] == c_block_type_water)
				wetness= c_min_wetness_for_grass_to_exist;

			// Count not fire blocks nearby, but nearby and above.
			if(z < c_chunk_height - 1 && chunks_input_data[adjacent_block_address + 1] == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address + 1]);
		}

		if(block_above_type == c_block_type_fire)
			total_fire_power_nearby+= int(chunks_auxiliar_input_data[column_address + z_up_clamped]);
		// Do not count fire below yellow grass block.

		// Require more fire power to burn grass in order to minimize fire disappearing when block is burned.
		if(total_fire_power_nearby >= c_min_fire_power_for_blocks_burning * 2)
		{
			// Randomly burn this yellow grass block into soil.
			if((block_rand & 15) == 0)
				return u8vec2(c_block_type_soil, 0);
		}

		if(wetness >= c_min_wetness_for_grass_to_exist)
		{
			// If wentess is high enough - randomly make yellow grass green.
			if((block_rand & 15) == 0)
				return u8vec2(c_block_type_grass, 0);
		}
	}
	else if(block_type == c_block_type_foliage)
	{
		// Foliage block - propagate special number with name "foliage factor".
		// This propagation is similar to light propagation.
		// Foliage blocks near to wood blocks has maximum foliage factor.
		// Other foliage blocks have maximum foliage factor of adjacent blocks minus one.

		int max_adjacent_foliage_factor= 0;
		int total_fire_power_nearby= 0;
		for(int i= 0; i < 6; ++i)
		{
			int adjacent_block_address= adjacent_columns[i] + z;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			if(adjacent_block_type == c_block_type_wood)
				max_adjacent_foliage_factor= c_max_foliage_factor;
			else if(adjacent_block_type == c_block_type_foliage)
			{
				int adjacent_foliage_factor= int(chunks_auxiliar_input_data[adjacent_block_address]);
				max_adjacent_foliage_factor= max(max_adjacent_foliage_factor, adjacent_foliage_factor);
			}
			else if(adjacent_block_type == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address]);
		}

		if(z < c_chunk_height - 1)
		{
			int adjacent_block_address= column_address + z + 1;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			if(adjacent_block_type == c_block_type_wood)
				max_adjacent_foliage_factor= c_max_foliage_factor;
			else if(adjacent_block_type == c_block_type_foliage)
			{
				int adjacent_foliage_factor= int(chunks_auxiliar_input_data[adjacent_block_address]);
				max_adjacent_foliage_factor= max(max_adjacent_foliage_factor, adjacent_foliage_factor);
			}
			else if(adjacent_block_type == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address]);
		}
		if(z > 0)
		{
			int adjacent_block_address= column_address + z - 1;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			if(adjacent_block_type == c_block_type_wood)
				max_adjacent_foliage_factor= c_max_foliage_factor;
			else if(adjacent_block_type == c_block_type_foliage)
			{
				int adjacent_foliage_factor= int(chunks_auxiliar_input_data[adjacent_block_address]);
				max_adjacent_foliage_factor= max(max_adjacent_foliage_factor, adjacent_foliage_factor);
			}
			else if(adjacent_block_type == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address]);
		}

		int this_block_foliage_factor= max(0, max_adjacent_foliage_factor - 1);

		const int border_size= c_max_foliage_factor + 2;
		if( block_x <= border_size || block_x >= max_coord.x - border_size ||
			block_y <= border_size || block_y >= max_coord.y - border_size)
		{
			// A workaround for world edges.
			// Force set foliage factor to maximum for blocks on world edges,
			// in order to prevent this block and adjacent blocks from disappearing.
			// This may happen, if corresponding wood block is outside current loaded world.
			this_block_foliage_factor= c_max_foliage_factor;
		}

		if(this_block_foliage_factor == 0)
		{
			// Randomly convert foliage with factor 0 into air.
			if((block_rand & 31) == 0)
				return u8vec2(c_block_type_air, uint8_t(0));
		}

		if(total_fire_power_nearby >= c_min_fire_power_for_blocks_burning)
		{
			if((block_rand & 15) == 0)
				return u8vec2(c_block_type_fire, uint8_t(c_initial_fire_power));
		}

		return u8vec2(block_type, uint8_t(this_block_foliage_factor));
	}
	else if(block_type == c_block_type_wood)
	{
		int total_fire_power_nearby= 0;
		for(int i= 0; i < 6; ++i)
		{
			int adjacent_block_address= adjacent_columns[i] + z;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			if(adjacent_block_type == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address]);
		}

		if(z < c_chunk_height - 1)
		{
			int adjacent_block_address= column_address + z + 1;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			if(adjacent_block_type == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address]);
		}
		if(z > 0)
		{
			int adjacent_block_address= column_address + z - 1;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			if(adjacent_block_type == c_block_type_fire)
				total_fire_power_nearby+= int(chunks_auxiliar_input_data[adjacent_block_address]);
		}

		if(total_fire_power_nearby >= c_min_fire_power_for_blocks_burning)
		{
			// Burn this wood  block if has fire nearby.
			if((block_rand & 63) == 0)
				return u8vec2(c_block_type_fire, uint8_t(c_initial_fire_power));
		}
	}
	else if(block_type == c_block_type_fire)
	{
		int total_flammability_nearby= 0;

		bool extinguish= false;
		for(int i= 0; i < 6; ++i) // For adjacent blocks.
		{
			int adjacent_block_address= adjacent_columns[i] + z;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			total_flammability_nearby+= int(c_block_flammability_table[uint(adjacent_block_type)]);
			if(adjacent_block_type == c_block_type_water)
			{
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
					extinguish= true;
			}
		}
		if(z < c_chunk_height - 1)
		{
			int adjacent_block_address= column_address + z + 1;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			total_flammability_nearby+= int(c_block_flammability_table[uint(adjacent_block_type)]);
			if(adjacent_block_type == c_block_type_water || adjacent_block_type == c_block_type_sand)
				extinguish= true;
		}
		if(z > 0)
		{
			int adjacent_block_address= column_address + z - 1;
			uint8_t adjacent_block_type= chunks_input_data[adjacent_block_address];
			total_flammability_nearby+= int(c_block_flammability_table[uint(adjacent_block_type)]);
		}

		if(total_flammability_nearby == 0 || extinguish)
		{
			// No flammable blocks nearby or has water nearby - immediately convert into air.
			return u8vec2(c_block_type_air, uint8_t(0));
		}

		// Each tick fire power is increased by total flammability nearby.
		int fire_power= int(chunks_auxiliar_input_data[column_address + z]);
		fire_power+= total_flammability_nearby;
		fire_power= min(fire_power, 255);

		return u8vec2(c_block_type_fire, uint8_t(fire_power));
	}
	else if(block_type == c_block_type_snow)
	{
		// Snow can exist only direct under sky and only if it's winter.
		int light_packed= light_data[column_address + z_up_clamped];
		int sky_light= light_packed >> c_sky_light_shift;

		bool can_exist=
			sky_light == c_max_sky_light &&
			z >= world_global_state.snow_z_level;

		if(!can_exist && (block_rand & 15) == 0)
			return u8vec2(c_block_type_air, uint8_t(0));

		// Immediately remove snow if block below can't be used for snow placement.
		if(!CanPlaceSnowOnThisBlock(chunks_input_data[column_address + z - 1]))
			return u8vec2(c_block_type_air, uint8_t(0));

		// Immediately remove snow if has too much heat.
		int fire_light= light_packed & c_fire_light_mask;
		if(fire_light > c_max_fire_light_for_snow_to_exist)
			return u8vec2(c_block_type_air, uint8_t(0));
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
