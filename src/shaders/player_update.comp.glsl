#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/world_blocks_external_update_queue.glsl"

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};

// This struct must be identical to the same struct in C++ code!
layout(binding= 1, std430) buffer player_state_buffer
{
	// Use vec4 for proper padding
	ivec4 build_pos; // .w - direction
	ivec4 destroy_pos;
};

layout(binding= 2, std430) buffer world_blocks_external_update_queue_buffer
{
	WorldBlocksExternalUpdateQueue world_blocks_external_update_queue;
};

layout(push_constant) uniform uniforms_block
{
	// Use vec4 for proper padding
	vec4 player_pos;
	vec4 player_dir;
	ivec2 world_size_chunks;
	// Use "uint8_t", because "bool" in GLSL has size different from C++.
	uint8_t build_block_type;
	uint8_t build_triggered;
	uint8_t destroy_triggered;
	uint8_t reserved[1];
};

uint8_t GetBuildDirection(ivec3 last_grid_pos, ivec3 grid_pos)
{
	if(last_grid_pos.z < grid_pos.z)
	{
		return c_direction_up;
	}
	else if(last_grid_pos.z > grid_pos.z)
	{
		return c_direction_down;
	}
	else if(last_grid_pos.x < grid_pos.x)
	{
		if((grid_pos.x & 1) == 0)
		{
			if(last_grid_pos.y == grid_pos.y)
			{
				return c_direction_north_east;
			}
			else
			{
				return c_direction_south_east;
			}
		}
		else
		{
			if(last_grid_pos.y == grid_pos.y)
			{
				return c_direction_south_east;
			}
			else
			{
				return c_direction_north_east;
			}
		}
	}
	else if(last_grid_pos.x > grid_pos.x)
	{
		if((grid_pos.x & 1) == 0)
		{
			if(last_grid_pos.y == grid_pos.y)
			{
				return c_direction_north_west;
			}
			else
			{
				return c_direction_south_west;
			}
		}
		else
		{
			if(last_grid_pos.y == grid_pos.y)
			{
				return c_direction_south_west;
			}
			else
			{
				return c_direction_north_west;
			}
		}
	}
	else
	{
		if(last_grid_pos.y < grid_pos.y)
		{
			return c_direction_north;
		}
		else
		{
			return c_direction_south;
		}
	}
}

void UpdateBuildPos()
{
	// For now just trace hex grid with fixed step to find nearest intersection.
	// TODO - perform more precise and not so slow search.
	const float c_build_radius= 5.0;
	const int c_num_steps= 64;

	float player_dir_len= length(player_dir);
	if(player_dir_len <= 0.0)
	{
		// Something went wrong.
		build_pos= ivec4(-1, -1, -1, 0);
		destroy_pos= ivec4(-1, -1, -1, 0);
		return;
	}

	vec3 player_dir_normalized= player_dir.xyz / player_dir_len;
	vec3 cur_pos= player_pos.xyz;
	vec3 step_vec= player_dir_normalized * (c_build_radius / float(c_num_steps));

	ivec3 last_grid_pos= ivec3(-1, -1, -1);
	for(int i= 0; i < c_num_steps; ++i, cur_pos+= step_vec)
	{
		ivec3 grid_pos= ivec3(GetHexogonCoord(cur_pos.xy), int(floor(cur_pos.z)));
		if(grid_pos == last_grid_pos)
			continue; // Located in the same grid cell.

		if(IsInWorldBorders(grid_pos, world_size_chunks) &&
			chunks_data[GetBlockFullAddress(grid_pos, world_size_chunks)] != c_block_type_air)
		{
			// Reached non-air block.
			// Destroy position is in this block, build position is in previous block.
			destroy_pos.xyz= grid_pos;
			build_pos.xyz= last_grid_pos;
			build_pos.w= int(GetBuildDirection(last_grid_pos, grid_pos));
			return;
		}

		last_grid_pos= grid_pos;
	}

	// Reached the end of the search - make build and destroy positions infinite.
	build_pos= ivec4(-1, -1, -1, 0);
	destroy_pos= ivec4(-1, -1, -1, 0);
}

void PushUpdateIntoQueue(WorldBlockExternalUpdate update)
{
	// Use atomic in case someone else pushes updates into this queue.
	uint index= atomicAdd(world_blocks_external_update_queue.num_updates, 1);
	if(index < c_max_world_blocks_external_updates)
	{
		world_blocks_external_update_queue.updates[index]= update;
	}
}

void main()
{
	UpdateBuildPos();

	// Perform building/destroying.
	// Update build pos if building/destroying was triggered.
	if(build_triggered != uint8_t(0) && IsInWorldBorders(build_pos.xyz, world_size_chunks))
	{
		WorldBlockExternalUpdate update;
		update.position= ivec4(build_pos.xyz, 0);
		update.old_block_type= chunks_data[GetBlockFullAddress(build_pos.xyz, world_size_chunks)];
		update.new_block_type= build_block_type;
		PushUpdateIntoQueue(update);

		UpdateBuildPos();
	}
	if(destroy_triggered != uint8_t(0) && IsInWorldBorders(destroy_pos.xyz, world_size_chunks))
	{
		WorldBlockExternalUpdate update;
		update.position= ivec4(destroy_pos.xyz, 0);
		update.old_block_type= chunks_data[GetBlockFullAddress(destroy_pos.xyz, world_size_chunks)];
		update.new_block_type= c_block_type_air;
		PushUpdateIntoQueue(update);

		UpdateBuildPos();
	}
}
