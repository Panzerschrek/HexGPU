#pragma once
#include "ChunkDataCompressor.hpp"
#include "Settings.hpp"
#include "WorldSaveLoad.hpp"
#include <array>
#include <optional>
#include <unordered_map>
#include <future>

namespace HexGPU
{

class ChunksStorage
{
public:
	// Global chunk coordinates.
	using ChunkCoord= std::array<int32_t, 2>;

public:
	explicit ChunksStorage(Settings& settings);
	~ChunksStorage();

	// Set current area of the world.
	// This may trigger regions loading and saving.
	void SetActiveArea(ChunkCoord start, std::array<uint32_t, 2> size);

	void SetChunk(ChunkCoord chunk_coord, ChunkDataCompresed data_compressed);

	// Returns non-null if has data for given chunk.
	// Result remains valid until next "SetActiveArea" call.
	const ChunkDataCompresed* GetChunk(ChunkCoord chunk_coord);

private:
	// Global coordinates of the first chunk.
	using RegionCoord= std::array<int32_t, 2>;

	struct RegionCoordHasher
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

	using LoadedRegionsList= std::vector<std::pair<RegionCoord, Region>>;
	using LoadedRegionsListPtr= std::shared_ptr<LoadedRegionsList>; // Use shared_ptr, because future::get returns copy, which is expensive
	using RegionsLoadingFuture= std::future<LoadedRegionsListPtr>;

private:
	static RegionCoord GetRegionCoordForChunk(ChunkCoord chunk_coord);
	static bool SaveRegion(const Region& region, const std::string& file_name);
	static std::optional<Region> LoadRegion(const std::string& file_name);
	static Region LoadOrCreateNewRegion(const std::string& file_name);

	// Finds region for given chunk and returns chunk data structure within this region.
	ChunkDataCompresed& GetChunkData(ChunkCoord chunk_coord);

	Region& EnsureRegionLoaded(RegionCoord region_coord);

	std::string GetRegionFilePath(RegionCoord region_coord) const;

	void TakeRegionsLoadingTaskResultIfReady();
	void EnsureRegionsLoadingTaskFinished();
	void PopulateRegionsMap(LoadedRegionsListPtr loaded_regions);

private:
	const std::string world_dir_path_;
	std::unordered_map<ChunkCoord, Region, RegionCoordHasher> regions_map_;

	RegionsLoadingFuture regions_loading_future_;
};

} // namespace HexGPU
