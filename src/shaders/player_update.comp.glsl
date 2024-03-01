#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/player_world_window.glsl"
#include "inc/keyboard.glsl"
#include "inc/matrix.glsl"
#include "inc/mouse.glsl"
#include "inc/world_blocks_external_update_queue.glsl"

// This struct must be identical to the same struct in C++ code!
layout(binding= 1, std430) buffer player_state_buffer
{
	mat4 blocks_matrix;
	vec4 player_pos;
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
	vec4 player_angles; // azimuth, elevation, aspect
	ivec2 world_size_chunks;
	float time_delta_s;
	uint keyboard_state;
	uint mouse_state;
	// Use "uint8_t", because "bool" in GLSL has size different from C++.
	uint8_t build_block_type;
};

void MovePlayer()
{
	const float speed= 4.0f;
	const float jump_speed= 0.8f * speed;
	const float angle_speed= 1.0f;

	vec3 forward_vector= vec3(-sin(player_angles.x), +cos(player_angles.x), 0.0);
	vec3 left_vector= vec3(cos(player_angles.x), sin(player_angles.x), 0.0);

	vec3 move_vector= vec3(0.0, 0.0, 0.0);

	if((keyboard_state & c_key_mask_forward) != 0)
		move_vector+= forward_vector;
	if((keyboard_state & c_key_mask_backward) != 0)
		move_vector-= forward_vector;
	if((keyboard_state & c_key_mask_step_left) != 0)
		move_vector+= left_vector;
	if((keyboard_state & c_key_mask_step_right) != 0)
		move_vector-= left_vector;

	const float move_vector_length= length(move_vector);
	if(move_vector_length > 0.0f)
		player_pos.xyz+= move_vector * (time_delta_s * speed / move_vector_length);

	if((keyboard_state & c_key_mask_fly_up) != 0)
		player_pos.z+= time_delta_s * jump_speed;
	if((keyboard_state & c_key_mask_fly_down) != 0)
		player_pos.z-= time_delta_s * jump_speed;
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

// Result vector is normalized.
vec3 CalculateCameraDirection(vec2 angles)
{
	float elevation_sin= sin(angles.y);
	float elevation_cos= cos(angles.y);

	return vec3(-sin(angles.x) * elevation_cos, +cos(angles.x) * elevation_cos, elevation_sin);
}

void UpdateBuildPos()
{
	// For now just trace hex grid with fixed step to find nearest intersection.
	// TODO - perform more precise and not so slow search.
	const float c_build_radius= 5.0;
	const int c_num_steps= 64;

	vec3 player_dir_normalized= CalculateCameraDirection(player_angles.xy);
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

void UpdateBlocksMatrix()
{
	const float z_near= 0.125;
	const float z_far= 1024.0;
	const float fov_deg= 75.0;

	const float fov= fov_deg * (3.1415926535 / 180.0);

	float aspect= player_angles.z;
	float fov_y= fov;

	mat4 perspective= MakePerspectiveProjectionMatrix(aspect, fov, z_near, z_far);
	mat4 basis_change= MakePerspectiveChangeBasisMatrix();
	mat4 rotate_x= MakeRotationXMatrix(-player_angles.y);
	mat4 rotate_z= MakeRotationZMatrix(-player_angles.x);
	mat4 translate= MateTranslateMatrix(-player_pos.xyz);
	mat4 blocks_scale= MakeScaleMatrix(vec3(0.5 / sqrt(3.0), 0.5, 1.0));

	blocks_matrix= perspective * basis_change * rotate_x * rotate_z * translate * blocks_scale;
}

void main()
{
	MovePlayer();

	UpdateBuildPos();

	// Perform building/destroying.
	// Update build pos if building/destroying was triggered.
	if((mouse_state & (1 << c_mouse_r_clicked_bit)) != 0)
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
	if((mouse_state & (1 << c_mouse_l_clicked_bit)) != 0)
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
