
// This struct must be identical to the same struct in C++ code!
struct WorldBlockExternalUpdate
{
	ivec4 position; // global
	uint8_t old_block_type;
	uint8_t new_block_type;
	uint8_t reserved[2];
};

const uint c_max_world_blocks_external_updates= 64;

// This struct must be identical to the same struct in C++ code!
struct WorldBlocksExternalUpdateQueue
{
	uint num_updates;
	WorldBlockExternalUpdate updates[c_max_world_blocks_external_updates];
};
