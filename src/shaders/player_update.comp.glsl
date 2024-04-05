#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/keyboard.glsl"
#include "inc/matrix.glsl"
#include "inc/mouse.glsl"
#include "inc/player_physics.glsl"
#include "inc/player_state.glsl"
#include "inc/player_world_window.glsl"
#include "inc/world_blocks_external_update_queue.glsl"
#include "inc/world_global_state.glsl"

layout(push_constant) uniform uniforms_block
{
	float aspect;
	float time_delta_s;
	uint keyboard_state;
	uint mouse_state;
	vec2 mouse_move;
	float fog_distance;
	uint8_t selected_block_type;
};

layout(binding= 1, std430) buffer player_state_buffer
{
	PlayerState player_state;
};

layout(binding= 2, std430) buffer world_blocks_external_update_queue_buffer
{
	WorldBlocksExternalUpdateQueue world_blocks_external_update_queue;
};

layout(binding= 3, std430) buffer player_world_window_buffer
{
	PlayerWorldWindow player_world_window;
};

layout(binding= 4, std430) readonly buffer world_global_state_buffer
{
	WorldGlobalState world_global_state;
};

// Player movement constants.
const float c_angle_speed= 1.0;
const float c_acceleration= 80.0;
const float c_deceleration= 40.0;
const float c_max_speed= 5.0;
const float c_max_sprint_speed= 20.0;
const float c_max_vertical_speed= 10.0;
const float c_max_vertical_sprint_speed= 20.0;

const float c_player_radius= 0.25 * 0.9; // 90% of block side
const float c_player_eyes_level= 1.65;
const float c_player_height= 1.75;

const float c_fov_y= radians(75.0);

void ProcessPlayerRotateInputs()
{
	player_state.angles.xy+= mouse_move;

	if((keyboard_state & c_key_mask_rotate_left) != 0)
		player_state.angles.x+= time_delta_s * c_angle_speed;
	if((keyboard_state & c_key_mask_rotate_right) != 0)
		player_state.angles.x-= time_delta_s * c_angle_speed;

	if((keyboard_state & c_key_mask_rotate_up) != 0)
		player_state.angles.y+= time_delta_s * c_angle_speed;
	if((keyboard_state & c_key_mask_rotate_down) != 0)
		player_state.angles.y-= time_delta_s * c_angle_speed;

	while(player_state.angles.x > +c_pi)
		player_state.angles.x-= 2.0 * c_pi;
	while(player_state.angles.x < -c_pi)
		player_state.angles.x+= 2.0 * c_pi;

	player_state.angles.y= max(-0.5 * c_pi, min(player_state.angles.y, +0.5 * c_pi));
}

void ProcessPlayerMoveInputs()
{
	vec3 forward_vector= vec3(-sin(player_state.angles.x), +cos(player_state.angles.x), 0.0);
	vec3 left_vector= vec3(cos(player_state.angles.x), sin(player_state.angles.x), 0.0);

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
	if(move_vector_length > 0.0)
	{
		move_vector/= move_vector_length;

		// Increate velocity if acceleration is applied.
		// Limit maximum velocity.
		float max_speed= (keyboard_state & c_key_mask_sprint) != 0 ? c_max_sprint_speed : c_max_speed;

		float velocity_projection_to_move_vector= dot(move_vector, player_state.velocity.xyz);
		if(velocity_projection_to_move_vector < max_speed)
		{
			float max_can_add= max_speed - velocity_projection_to_move_vector;
			player_state.velocity.xyz+= move_vector * min(c_acceleration * time_delta_s, max_can_add);
		}

	}

	float move_up_vector= 0.0;

	if((keyboard_state & c_key_mask_fly_up) != 0)
		move_up_vector+= 1.0;
	if((keyboard_state & c_key_mask_fly_down) != 0)
		move_up_vector-= 1.0;

	float max_vertical_speed=
		(keyboard_state & c_key_mask_sprint) != 0 ? c_max_vertical_sprint_speed : c_max_vertical_speed;

	// Increate vertical velocity if acceleration is applied.
	player_state.velocity.z+= c_acceleration * move_up_vector * time_delta_s;
	if(player_state.velocity.z > max_vertical_speed)
		player_state.velocity.z= max_vertical_speed;
	else if(player_state.velocity.z < -max_vertical_speed)
		player_state.velocity.z= -max_vertical_speed;
}

vec3 CollidePlayerAgainstWorld(vec3 old_pos, vec3 new_pos)
{
	ivec3 grid_pos= ivec3(GetHexogonCoord(new_pos.xy), int(floor(new_pos.z)));

	ivec3 pos_in_window= grid_pos - player_world_window.offset.xyz;

	float player_min_z= new_pos.z;
	float player_max_z= new_pos.z + c_player_height;

	vec3 move_ray= new_pos - old_pos;
	float move_ray_length= length(move_ray);

	// Find closest contact point.
	CollisionDetectionResult result;
	result.normal= vec3(0.0, 0.0, 1.0);
	result.move_dist= move_ray_length;

	// Check only nearby blocks.
	// Increase this volume if player radius bacomes bigger.
	for(int dx= -1; dx <= 1; ++dx)
	for(int dy= -1; dy <= 1; ++dy)
	for(int dz= -1; dz <= 2; ++dz)
	{
		ivec3 block_pos_in_window= pos_in_window + ivec3(dx, dy, dz);
		if(!IsPosInsidePlayerWorldWindow(block_pos_in_window))
			continue;

		uint8_t block_type= player_world_window.window_data[GetAddressOfBlockInPlayerWorldWindow(block_pos_in_window)];
		if(c_block_optical_density_table[uint(block_type)] == c_optical_density_air)
			continue;

		ivec3 block_global_coord= block_pos_in_window + player_world_window.offset.xyz;

		CollisionDetectionResult block_collision_result=
			CollideCylinderWithBlock(old_pos, new_pos, c_player_radius, c_player_height, block_global_coord);
		if(block_collision_result.move_dist < result.move_dist)
		{
			result.move_dist= block_collision_result.move_dist;
			result.normal= block_collision_result.normal;
		}
	}

	if(result.move_dist >= move_ray_length)
		return new_pos;

	vec3 intersection_pos= old_pos + move_ray * (result.move_dist / move_ray_length);

	float move_inside_length= move_ray_length - result.move_dist;
	if(move_inside_length > 0.0)
	{
		vec3 move_inside_vec= move_ray * (move_inside_length / move_ray_length);

		// Clamp movement inside a surface by its normal.
		float move_inside_vec_normal_dot= dot(move_inside_vec, result.normal);
		if(move_inside_vec_normal_dot < 0.0)
		{
			// TODO - clamp also speed.
			vec3 move_inside_clamped= move_inside_vec - result.normal * move_inside_vec_normal_dot;
			return intersection_pos + move_inside_clamped;
		}
		else
			return intersection_pos;
	}
	else
		return intersection_pos;
}

void MovePlayer()
{
	// Apply velocity to position.
	vec3 new_pos= player_state.pos.xyz + player_state.velocity.xyz * time_delta_s;

	// Perform collisions.
	for(int i= 0; i < 4; ++i)
		new_pos= CollidePlayerAgainstWorld(player_state.pos.xyz, new_pos);
	player_state.pos.xyz= new_pos;

	// Decelerate player.
	// Do this only after applying velocity to position.
	{
		float speed= length(player_state.velocity.xyz);
		if(speed > 0.0)
		{
			float decelearion= c_deceleration * time_delta_s;
			if(decelearion >= speed)
				player_state.velocity.xyz= vec3(0.0, 0.0, 0.0);
			else
			{
				float new_speed= speed - decelearion;
				player_state.velocity.xyz= player_state.velocity.xyz * (new_speed / speed);
			}
		}
	}
}

void UpdateBuildBlockType()
{
	if(selected_block_type != c_block_type_air)
		player_state.build_block_type= selected_block_type;
}

uint8_t GetBuildDirection(ivec3 last_grid_pos, ivec3 grid_pos)
{
	if(last_grid_pos.z < grid_pos.z)
		return c_direction_up;
	else if(last_grid_pos.z > grid_pos.z)
		return c_direction_down;
	else if(last_grid_pos.x < grid_pos.x)
	{
		if((grid_pos.x & 1) == 0)
		{
			if(last_grid_pos.y == grid_pos.y)
				return c_direction_north_east;
			else
				return c_direction_south_east;
		}
		else
		{
			if(last_grid_pos.y == grid_pos.y)
				return c_direction_south_east;
			else
				return c_direction_north_east;
		}
	}
	else if(last_grid_pos.x > grid_pos.x)
	{
		if((grid_pos.x & 1) == 0)
		{
			if(last_grid_pos.y == grid_pos.y)
				return c_direction_north_west;
			else
				return c_direction_south_west;
		}
		else
		{
			if(last_grid_pos.y == grid_pos.y)
				return c_direction_south_west;
			else
				return c_direction_north_west;
		}
	}
	else
	{
		if(last_grid_pos.y < grid_pos.y)
			return c_direction_north;
		else
			return c_direction_south;
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

	vec3 player_dir_normalized= CalculateCameraDirection(player_state.angles.xy);
	vec3 cur_pos= vec3(player_state.pos.xy, player_state.pos.z + c_player_eyes_level);
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
			uint8_t block_type= player_world_window.window_data[GetAddressOfBlockInPlayerWorldWindow(pos_in_window)];
			if(c_block_optical_density_table[uint(block_type)] != c_optical_density_air)
			{
				// Reached non-solid block.
				// Destroy position is in this block, build position is in previous block.
				player_state.destroy_pos.xyz= grid_pos;
				player_state.build_pos.xyz= last_grid_pos;
				player_state.build_pos.w= int(GetBuildDirection(last_grid_pos, grid_pos));
				return;
			}
		}

		last_grid_pos= grid_pos;
	}

	// Reached the end of the search - make build and destroy positions infinite.
	player_state.build_pos= ivec4(-1, -1, -1, 0);
	player_state.destroy_pos= ivec4(-1, -1, -1, 0);
}

void PushUpdateIntoQueue(WorldBlockExternalUpdate update)
{
	// Use atomic in case someone else pushes updates into this queue.
	uint index= atomicAdd(world_blocks_external_update_queue.num_updates, 1);
	if(index < c_max_world_blocks_external_updates)
		world_blocks_external_update_queue.updates[index]= update;
}

void UpdatePlayerMatrices()
{
	const float z_near= 0.075; // TODO - calculate it based on FOV and nearby colliders.
	const float z_far= 2048.0;

	mat4 rotation_and_perspective=
		MakePerspectiveProjectionMatrix(aspect, c_fov_y, z_near, z_far) *
		MakePerspectiveChangeBasisMatrix() *
		MakeRotationXMatrix(-player_state.angles.y) *
		MakeRotationZMatrix(-player_state.angles.x);

	mat4 translate_and_blocks_scale=
		MateTranslateMatrix(-vec3(player_state.pos.xy, player_state.pos.z + c_player_eyes_level)) *
		MakeScaleMatrix(vec3(0.5 / sqrt(3.0), 0.5, 1.0 / 256.0));

	player_state.blocks_matrix= rotation_and_perspective * translate_and_blocks_scale;

	// Do not upply translation to sky matrix - always keep player in the center of the sky mesh.
	player_state.sky_matrix= rotation_and_perspective;

	// Rotate stars by special matrix from global world state.
	player_state.stars_matrix= rotation_and_perspective * world_global_state.stars_matrix;

	// For fog matrix only translation and scale are required.
	// Rotation isn't needed, since fog is spherical.
	player_state.fog_matrix= MakeScaleMatrix(vec3(1.0 / fog_distance)) * translate_and_blocks_scale;
}

void UpdatePlayerFrustumPlanes()
{
	mat3 rotation_matrix=
		mat3(MakeRotationZMatrix(player_state.angles.x)) * mat3(MakeRotationXMatrix(player_state.angles.y));

	vec3 front_plane_normal= rotation_matrix * vec3(0.0, -1.0, 0.0);

	float half_fov_y= c_fov_y * 0.5;
	vec3 upper_plane_normal= rotation_matrix * vec3(0.0, -sin(half_fov_y),  cos(half_fov_y));
	vec3 lower_plane_normal= rotation_matrix * vec3(0.0, -sin(half_fov_y), -cos(half_fov_y));

	float half_fov_x= atan(tan(half_fov_y) * aspect);
	vec3 left_plane_normal = rotation_matrix * vec3(-cos(half_fov_x), -sin(half_fov_x), 0.0);
	vec3 right_plane_normal= rotation_matrix * vec3( cos(half_fov_x), -sin(half_fov_x), 0.0);

	player_state.frustum_planes[0]= vec4(front_plane_normal, -dot(front_plane_normal, player_state.pos.xyz));
	player_state.frustum_planes[1]= vec4(upper_plane_normal, -dot(upper_plane_normal, player_state.pos.xyz));
	player_state.frustum_planes[2]= vec4(lower_plane_normal, -dot(lower_plane_normal, player_state.pos.xyz));
	player_state.frustum_planes[3]= vec4(left_plane_normal , -dot(left_plane_normal , player_state.pos.xyz));
	player_state.frustum_planes[4]= vec4(right_plane_normal, -dot(right_plane_normal, player_state.pos.xyz));
}

void UpdateNextPlayerWorldWindowOffset()
{
	player_state.next_player_world_window_offset=
		ivec4(
			(GetHexogonCoord(player_state.pos.xy) - c_player_world_window_size.xy / 2) & 0xFFFFFFFE,
			int(floor(player_state.pos.z)) - c_player_world_window_size.z / 2,
			0);
}

void UpdateFogColor()
{
	// Multiply fog color by factor dependent on sky light at player position in order to make fog darker underground.
	float sky_light= float(player_world_window.player_block_light >> c_sky_light_shift) / float(c_max_sky_light);
	float fog_brightness= pow(sky_light, 0.333); // Use non-linear function to keep fog brighter in semi-dark places.
	vec3 new_fog_color= fog_brightness * world_global_state.base_fog_color.rgb;

	// Perform temporal interpolation of fog color.
	// Calculate mix factor based on time delata in order to keep update speed constant.
	float mix_factor= pow(0.3, time_delta_s);
	player_state.fog_color.rgb= mix(new_fog_color, player_state.fog_color.rgb, mix_factor);
}

void main()
{
	ProcessPlayerRotateInputs();
	ProcessPlayerMoveInputs();
	MovePlayer();

	UpdateBuildBlockType();

	UpdateBuildPos();

	// Perform building/destroying.
	// Update build pos if building/destroying was triggered.
	if((mouse_state & c_mouse_mask_r_clicked) != 0)
	{
		ivec3 pos_in_window= player_state.build_pos.xyz - player_world_window.offset.xyz;
		if(IsPosInsidePlayerWorldWindow(pos_in_window))
		{
			int address_in_window= GetAddressOfBlockInPlayerWorldWindow(pos_in_window);
			uint8_t destination_block_type= player_world_window.window_data[address_in_window];
			if( destination_block_type == c_block_type_air ||
				destination_block_type == c_block_type_fire ||
				destination_block_type == c_block_type_snow)
			{
				WorldBlockExternalUpdate update;
				update.position= ivec4(player_state.build_pos.xyz, 0);
				update.old_block_type= player_world_window.window_data[address_in_window];
				update.new_block_type= player_state.build_block_type;
				PushUpdateIntoQueue(update);

				player_world_window.window_data[address_in_window]= player_state.build_block_type;
				UpdateBuildPos();
			}
		}
	}
	if((mouse_state & c_mouse_mask_l_clicked) != 0)
	{
		ivec3 pos_in_window= player_state.destroy_pos.xyz - player_world_window.offset.xyz;
		if(IsPosInsidePlayerWorldWindow(pos_in_window))
		{
			int address_in_window= GetAddressOfBlockInPlayerWorldWindow(pos_in_window);
			if(player_world_window.window_data[address_in_window] != c_block_type_water)
			{
				WorldBlockExternalUpdate update;
				update.position= ivec4(player_state.destroy_pos.xyz, 0);
				update.old_block_type= player_world_window.window_data[address_in_window];
				update.new_block_type= c_block_type_air;
				PushUpdateIntoQueue(update);

				player_world_window.window_data[address_in_window]= c_block_type_air;
				UpdateBuildPos();
			}
		}
	}

	UpdatePlayerMatrices();
	UpdatePlayerFrustumPlanes();
	UpdateNextPlayerWorldWindowOffset();
	UpdateFogColor();
}
