const ivec3 c_player_world_window_size= ivec3(16, 16, 16);
const int c_player_world_window_volume= c_player_world_window_size.x * c_player_world_window_size.y * c_player_world_window_size.z;

// Some part of the world around the player.
// Used to calculate updates of the world from the player.
// Updating the world directly isn't possible, because world updating is asynchronous to player logic.
// This struct must be identical to the same struct in C++ code!
struct PlayerWorldWindow
{
	ivec4 offset; // Position of the window start (in blocks)
	uint8_t window_data[c_player_world_window_volume];
};
