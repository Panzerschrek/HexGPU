// If this changed, the same struct in C++ code must be changed too!
struct ChunkDrawInfo
{
	uint num_quads;
	uint new_num_quads;
	uint first_quad; // Index in total buffer.
	uint num_water_quads;
	uint new_water_num_quads;
	uint first_water_quad; // Index in total buffer.
	uint num_fire_quads;
	uint new_fire_num_quads;
	uint first_fire_quad;
	uint first_memory_unit;
	uint num_memory_units;
};
