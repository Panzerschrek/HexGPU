struct ChunkStructureDescription
{
	// Coordinates - relative to chunk.
	i8vec4 min; // w - structure kind id
	i8vec4 max;
};

const uint c_max_chunk_structures= 24;

struct ChunkGenInfo
{
	uint num_structures;
	uint reserved[3];
	ChunkStructureDescription structures[c_max_chunk_structures];
};
