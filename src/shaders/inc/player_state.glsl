// This struct must be identical to the same struct in C++ code!
struct PlayerState
{
	mat4 blocks_matrix;
	mat4 fog_matrix;
	mat4 sky_matrix;
	mat4 stars_matrix;
	vec4 fog_color;
	vec4 frustum_planes[5];
	vec4 pos;
	vec4 angles; // azimuth, elevation
	vec4 velocity;
	// Use vec4 for proper padding
	// positions are global
	ivec4 build_pos; // .w - direction
	ivec4 destroy_pos;
	ivec4 next_player_world_window_offset;
	uint8_t build_block_type;
};
