#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/player_world_window.glsl"
#include "inc/world_blocks_external_update_queue.glsl"

// This struct must be identical to the same struct in C++ code!
layout(binding= 1, std430) buffer player_state_buffer
{
	mat4 blocks_matrix;
	// Use vec4 for proper padding
	// positions are global
	ivec4 build_pos; // .w - direction
	ivec4 destroy_pos;
};

layout(binding= 2, std430) buffer world_blocks_external_update_queue_buffer
{
	WorldBlocksExternalUpdateQueue world_blocks_external_update_queue;
};

layout(binding= 3, std430) buffer player_world_window_buffer
{
	PlayerWorldWindow player_world_window;
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

		ivec3 pos_in_window= grid_pos - player_world_window.offset.xyz;
		if(IsPosInsidePlayerWorldWindow(pos_in_window))
		{
			if(player_world_window.window_data[GetAddressOfBlockInPlayerWorldWindow(pos_in_window)] != c_block_type_air)
			{
				// Reached non-air block.
				// Destroy position is in this block, build position is in previous block.
				destroy_pos.xyz= grid_pos;
				build_pos.xyz= last_grid_pos;
				build_pos.w= int(GetBuildDirection(last_grid_pos, grid_pos));
				return;
			}
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

mat4 MakePerspectiveProjectionMatrix(float aspect, float fov_y, float z_near, const float z_far)
{
	const float inv_half_fov_tan= 1.0 / tan(fov_y * 0.5);

	mat4 m= mat4(0.0);

	m[0][0]= inv_half_fov_tan / aspect;
	m[1][1]= inv_half_fov_tan;

	// Vulkan depth range [0; 1]
	m[3][2]= 1.0 / (1.0 / z_far - 1.0 / z_near);
	m[2][2]= 1.0 / (1.0 - z_near / z_far);
	m[2][3]= 1.0;

	m[0][1]= m[0][2]= m[0][3]= 0.0;
	m[1][0]= m[1][2]= m[1][3]= 0.0;
	m[2][0]= m[2][1]= 0.0;
	m[3][0]= m[3][1]= m[3][3]= 0.0;

	return m;
}

mat4 MakePerspectiveChangeBasisMatrix()
{
	mat4 m= mat4(1.0); // identity.
	m[1][1]= 0.0;
	m[1][2]= 1.0;
	m[2][1]= -1.0;
	m[2][2]= 0.0;
	return m;
}

mat4 MateTranslateMatrix(vec3 shift)
{
	mat4 m= mat4(1.0); // identity.
	m[3][0]= shift.x;
	m[3][1]= shift.y;
	m[3][2]= shift.z;
	return m;
}

mat4 MakeScaleMatrix(vec3 scale)
{
	mat4 m= mat4(1.0); // identity
	m[0][0]= scale.x;
	m[1][1]= scale.y;
	m[2][2]= scale.z;
	return m;
}

void UpdateBlocksMatrix()
{
	const float z_near= 0.125;
	const float z_far= 1024.0;
	const float fov_deg= 75.0;

	const float fov= fov_deg * (3.1415926535 / 180.0);

	float aspect= 1.0;
	float fov_y= fov;

	mat4 perspective= MakePerspectiveProjectionMatrix(aspect, fov, z_near, z_far);
	mat4 basis_change= MakePerspectiveChangeBasisMatrix();
	mat4 translate= MateTranslateMatrix(-player_pos.xyz);
	mat4 blocks_scale= MakeScaleMatrix(vec3(0.5 / sqrt(3.0), 0.5, 1.0));

	blocks_matrix= perspective * basis_change * translate * blocks_scale;
}

void main()
{
	UpdateBuildPos();

	// Perform building/destroying.
	// Update build pos if building/destroying was triggered.
	if(build_triggered != uint8_t(0))
	{
		ivec3 pos_in_window= build_pos.xyz - player_world_window.offset.xyz;
		if(IsPosInsidePlayerWorldWindow(pos_in_window))
		{
			int address_in_window= GetAddressOfBlockInPlayerWorldWindow(pos_in_window);

			WorldBlockExternalUpdate update;
			update.position= ivec4(build_pos.xyz, 0);
			update.old_block_type= player_world_window.window_data[address_in_window];
			update.new_block_type= build_block_type;
			PushUpdateIntoQueue(update);

			player_world_window.window_data[address_in_window]= build_block_type;
			UpdateBuildPos();
		}
	}
	if(destroy_triggered != uint8_t(0))
	{
		ivec3 pos_in_window= destroy_pos.xyz - player_world_window.offset.xyz;
		if(IsPosInsidePlayerWorldWindow(pos_in_window))
		{
			int address_in_window= GetAddressOfBlockInPlayerWorldWindow(pos_in_window);

			WorldBlockExternalUpdate update;
			update.position= ivec4(destroy_pos.xyz, 0);
			update.old_block_type= player_world_window.window_data[address_in_window];
			update.new_block_type= c_block_type_air;
			PushUpdateIntoQueue(update);

			player_world_window.window_data[address_in_window]= c_block_type_air;
			UpdateBuildPos();
		}
	}

	UpdateBlocksMatrix();
}
