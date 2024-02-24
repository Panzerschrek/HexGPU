// If this changed, the same struct in C++ code must be changed too!
struct ChunkDrawInfo
{
	uint num_quads;
	uint new_num_quads;
	uint first_quad; // Index in total buffer.
	uint first_memory_unit;
	uint num_memory_units;
};
