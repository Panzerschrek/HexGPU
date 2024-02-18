#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

// This struct must be identical to the same struct in C++ code!
layout(binding= 1, std430) buffer player_state_buffer
{
	// Use vec4 for proper padding
	ivec4 build_pos; // .w - direction
	ivec4 destroy_pos;
};

layout(push_constant) uniform uniforms_block
{
	// Use vec4 for proper padding
	vec4 player_pos;
	vec4 player_dir;
	// Use "uint8_t", because "bool" in GLSL has size different from C++.
	uint8_t build_triggered;
	uint8_t destroy_triggered;
	uint8_t reserved[2];
};

bool IsInWorldBorders(ivec3 pos)
{
	return
		pos.x >= 0 && pos.x < c_chunk_width * c_chunk_matrix_size[0] &&
		pos.y >= 0 && pos.y < c_chunk_width * c_chunk_matrix_size[1] &&
		pos.z >= 0 && pos.z < c_chunk_height;
}

// Block must be in world borders!
int GetBlockFullAddress(ivec3 pos)
{
	int chunk_x= pos.x >> c_chunk_width_log2;
	int chunk_y= pos.y >> c_chunk_width_log2;
	int local_x= pos.x & (c_chunk_width - 1);
	int local_y= pos.y & (c_chunk_width - 1);

	int chunk_index= chunk_x + chunk_y * c_chunk_matrix_size[0];
	int chunk_data_offset= chunk_index * c_chunk_volume;

	return chunk_data_offset + ChunkBlockAddress(local_x, local_y, pos.z);
}

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

		if(IsInWorldBorders(grid_pos) &&
			chunks_data[GetBlockFullAddress(grid_pos)] != c_block_type_air)
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

void main()
{
	UpdateBuildPos();

	if(build_triggered != uint8_t(0) && IsInWorldBorders(build_pos.xyz))
	{
		chunks_data[GetBlockFullAddress(build_pos.xyz)]= c_block_type_fire_stone;
	}
	if(destroy_triggered != uint8_t(0) && IsInWorldBorders(destroy_pos.xyz))
	{
		chunks_data[GetBlockFullAddress(destroy_pos.xyz)]= c_block_type_air;
	}
}
