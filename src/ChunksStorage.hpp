#pragma once
#include "Constants.hpp"
#include "BlockType.hpp"
#include <unordered_map>

namespace HexGPU
{

class ChunksStorage
{
public:
	// Global chunk coordinates.
	using ChunkCoord= std::array<int32_t, 2>;

public:
	// Input arrays are of size c_chunk_volume.
	void SetChunk(ChunkCoord chunk_coord, const BlockType* blocks_data, const uint8_t* blocks_auxiliar_data);

	// Returns true if found.
	// Fills output arrays of size c_chunk_volume on success.
	bool GetChunk(ChunkCoord coord, BlockType* blocks_data, uint8_t* blocks_auxiliar_data);

	bool HasDataForChunk(ChunkCoord coord);

private:
	struct ChunkDataCompresed
	{
		std::string blocks;
		std::string auxiliar_data;
	};

	struct ChunkCoordHasher
	{
		size_t operator()(const ChunkCoord& coord) const
		{
			if(sizeof(size_t) >= 2 * sizeof(int32_t))
				return size_t(coord[0]) | (size_t(coord[1]) << 32);
			else
				return size_t(coord[0]) | (size_t(coord[1]) << 16);
		}
	};

private:
	std::unordered_map<ChunkCoord, ChunkDataCompresed, ChunkCoordHasher> chunks_map_;
};

} // namespace HexGPU
