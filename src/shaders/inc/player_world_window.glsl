// These constants must be the same in GLSL code!
const ivec3 c_player_world_window_size= ivec3(16, 16, 16);
const int c_player_world_window_volume= c_player_world_window_size.x * c_player_world_window_size.y * c_player_world_window_size.z;

// Some part of the world around the player.
// Used to calculate updates of the world from the player.
// Updating the world directly isn't possible, because world updating is asynchronous to player logic.
// This struct must be identical to the same struct in C++ code!
struct PlayerWorldWindow
{
	ivec4 offset; // Position of the window start (in blocks)
	uint player_block_light;
	uint8_t window_data[c_player_world_window_volume];
};

bool IsPosInsidePlayerWorldWindow(ivec3 pos_relative_window)
{
	return
		pos_relative_window.x >= 0 && pos_relative_window.x < c_player_world_window_size.x &&
		pos_relative_window.y >= 0 && pos_relative_window.y < c_player_world_window_size.y &&
		pos_relative_window.z >= 0 && pos_relative_window.z < c_player_world_window_size.z;
}

int GetAddressOfBlockInPlayerWorldWindow(ivec3 pos_in_window)
{
	return pos_in_window.z + pos_in_window.y * c_player_world_window_size.z + pos_in_window.x * (c_player_world_window_size.z * c_player_world_window_size.y);
}
