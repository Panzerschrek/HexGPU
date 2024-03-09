struct ChunkStructureDescription
{
	// Coordinates - relative to chunk.
	i16vec4 min;
	i16vec4 max;
};

const uint c_max_chunk_structures= 24;

struct ChunkGenInfo
{
	uint num_structures;
	uint reserved[3];
	ChunkStructureDescription structures[c_max_chunk_structures];
};
