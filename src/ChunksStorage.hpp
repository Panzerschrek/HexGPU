#pragma once
#include "ChunkDataCompressor.hpp"
#include <unordered_map>

namespace HexGPU
{

class ChunksStorage
{
public:
	// Global chunk coordinates.
	using ChunkCoord= std::array<int32_t, 2>;

public:
	void SetChunk(ChunkCoord chunk_coord, ChunkDataCompresed data_compressed);

	// returns non-null if has data for given chunk.
	const ChunkDataCompresed* GetChunk(ChunkCoord coord);

	bool HasDataForChunk(ChunkCoord coord);

private:
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
