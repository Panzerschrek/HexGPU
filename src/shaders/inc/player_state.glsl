// This struct must be identical to the same struct in C++ code!
struct PlayerState
{
	mat4 blocks_matrix;
	vec4 pos;
	vec4 angles; // azimuth, elevation
	// Use vec4 for proper padding
	// positions are global
	ivec4 build_pos; // .w - direction
	ivec4 destroy_pos;
	ivec4 next_player_world_window_offset;
	uint8_t build_block_type;
};
