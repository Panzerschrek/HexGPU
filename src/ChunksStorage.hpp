#pragma once
#include "ChunkDataCompressor.hpp"
#include "WorldSaveLoad.hpp"
#include <array>
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

	struct Region
	{
		ChunkDataCompresed chunks[c_world_region_area];
	};

private:
	static bool SaveRegion(const Region& region, const std::string& file_name);
	static std::optional<Region> LoadRegion(const std::string& file_name);

private:
	std::unordered_map<ChunkCoord, ChunkDataCompresed, ChunkCoordHasher> chunks_map_;
};

} // namespace HexGPU
