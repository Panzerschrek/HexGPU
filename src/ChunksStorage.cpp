#include "ChunksStorage.hpp"
#include "Log.hpp"
#include "Math.hpp"
#include <cstring>

namespace HexGPU
{

ChunksStorage::ChunksStorage(Settings& settings)
	: world_dir_path_(settings.GetOrSetString("g_world_dir", "world"))
{
}

ChunksStorage::~ChunksStorage()
{
	for(const auto& region_pair : regions_map_)
		SaveRegion(region_pair.second, GetRegionFilePath(region_pair.first));
}

void ChunksStorage::SetActiveArea(const ChunkCoord start, const std::array<uint32_t, 2> size)
{
	const RegionCoord min_coord= GetRegionCoordForChunk(start);
	const RegionCoord max_coord= GetRegionCoordForChunk(
		{start[0] + int32_t(size[0]) - 1, start[1] + int32_t(size[1]) - 1});

	// Load also regions at borders - in order to be ready to provide chunk data when it's already needed.
	const RegionCoord min_coord_extended{
		min_coord[0] - int32_t(c_world_region_size[0]),
		min_coord[1] - int32_t(c_world_region_size[1]) };

	const RegionCoord max_coord_extended{
		max_coord[0] + int32_t(c_world_region_size[0]),
		max_coord[1] + int32_t(c_world_region_size[1]) };

	// Free regions which are no longer inside active area.
	for(auto it= regions_map_.begin(); it != regions_map_.end();)
	{
		const RegionCoord& region_coord= it->first;

		const bool inside=
			region_coord[0] >= min_coord_extended[0] && region_coord[0] <= max_coord_extended[0] &&
			region_coord[1] >= min_coord_extended[1] && region_coord[1] <= max_coord_extended[1];

		if(inside)
		{
			// Keep this region and continue iteration.
			++it;
		}
		else
		{
			// Save this region, erase it from regions container and continue iteration.
			SaveRegion(it->second, GetRegionFilePath(region_coord));
			it= regions_map_.erase(it);
		}
	}

	// If previous async regions loading result is already there - take it.
	// But for now do not force to wait for loading.
	TakeRegionsLoadingTaskResultIfReady();

	// Check if we need to load new regions.
	std::vector<RegionCoord> regions_to_load;
	for(int32_t y= min_coord_extended[1]; y <= max_coord_extended[1]; y+= int32_t(c_world_region_size[1]))
	for(int32_t x= min_coord_extended[0]; x <= max_coord_extended[0]; x+= int32_t(c_world_region_size[0]))
	{
		const RegionCoord region_coord{x, y};

		if(regions_map_.count(region_coord) == 0)
			regions_to_load.push_back(region_coord);
	}

	if(regions_to_load.empty())
		return; // Nothing to load.

	EnsureRegionsLoadingTaskFinished(); // End previous task if it's still running.

	// Check for regions again. Some of them may be loaded in previous step, in such case remove them from the list.
	regions_to_load.erase(
		std::remove_if(
			regions_to_load.begin(), regions_to_load.end(),
			[this](const RegionCoord region_coord)
			{
				return regions_map_.count(region_coord) != 0;
			}),
		regions_to_load.end());

	if(regions_to_load.empty())
		return; // Nothing to load.

	// Can start the task.
	HEX_ASSERT(!regions_loading_future_.valid());

	regions_loading_future_= std::async(
		std::launch::async,
		[this, regions_to_load= std::move(regions_to_load)]
		{
			LoadedRegionsList loaded_regions;
			loaded_regions.reserve(regions_to_load.size());
			for(const RegionCoord& region_coord : regions_to_load)
				loaded_regions.emplace_back(region_coord, LoadOrCreateNewRegion(GetRegionFilePath(region_coord)));

			return std::make_shared<LoadedRegionsList>(std::move(loaded_regions));
		});
}

void ChunksStorage::SetChunk(const ChunkCoord chunk_coord, ChunkDataCompresed data_compressed)
{
	GetChunkData(chunk_coord)= std::move(data_compressed);
}

const ChunkDataCompresed* ChunksStorage::GetChunk(const ChunkCoord chunk_coord)
{
	const ChunkDataCompresed& chunk_data= GetChunkData(chunk_coord);
	if(!chunk_data.blocks.empty() && !chunk_data.auxiliar_data.empty())
		return &chunk_data;

	return nullptr;
}

ChunksStorage::RegionCoord ChunksStorage::GetRegionCoordForChunk(const ChunkCoord chunk_coord)
{
	RegionCoord res;
	for(uint32_t i= 0; i < 2; ++i)
		res[i]= EuclidianDiv(chunk_coord[i], int32_t(c_world_region_size[i])) * c_world_region_size[i];

	return res;
}

bool ChunksStorage::SaveRegion(const Region& region, const std::string& file_name)
{
	std::ofstream file(file_name, std::ios::binary);
	if(!file.is_open())
	{
		Log::Warning("Can't open file \"", file_name, "\"");
		return false;
	}

	uint32_t offset= 0;

	RegionFile::FileHeader file_header;

	std::memcpy(file_header.id, RegionFile::c_expected_id, sizeof(RegionFile::c_expected_id));
	file_header.version= RegionFile::c_expected_version;

	// Write header first to reserve place for it.
	file.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
	offset+= uint32_t(sizeof(file_header));

	for(uint32_t i= 0; i < c_world_region_area; ++i)
	{
		const ChunkDataCompresed& chunk_data= region.chunks[i];
		if(!chunk_data.blocks.empty() && !chunk_data.auxiliar_data.empty())
		{
			RegionFile::ChunkHeader& chunk_header= file_header.chunks[i];

			chunk_header.block_data_offset= offset;
			chunk_header.block_data_size= uint32_t(chunk_data.blocks.size());
			file.write(chunk_data.blocks.data(), chunk_data.blocks.size());
			offset+= uint32_t(chunk_data.blocks.size());

			chunk_header.auxiliar_data_offset= offset;
			chunk_header.auxiliar_data_size= uint32_t(chunk_data.auxiliar_data.size());
			file.write(chunk_data.auxiliar_data.data(), chunk_data.auxiliar_data.size());
			offset+= uint32_t(chunk_data.auxiliar_data.size());
		}
	}

	// Write the header again - to update offsets.
	file.seekp(0);
	file.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));

	file.flush();
	if(file.fail())
	{
		Log::Warning("Failed to flush region file!");
		return false;
	}

	return true;
}

std::optional<ChunksStorage::Region> ChunksStorage::LoadRegion(const std::string& file_name)
{
	std::ifstream file(file_name, std::ios::binary);
	if(!file.is_open())
	{
		// No file found.
		return std::nullopt;
	}

	RegionFile::FileHeader file_header;
	file.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));

	if(std::memcmp(file_header.id, RegionFile::c_expected_id, sizeof(RegionFile::c_expected_id)) != 0)
	{
		Log::Warning("File \"", file_name, "\" seems to be not a valid region file");
		return std::nullopt;
	}

	if(file_header.version != RegionFile::c_expected_version)
	{
		Log::Warning("Region file version mismatch, expected ", RegionFile::c_expected_version, ", got ", file_header.version);
		return std::nullopt;
	}

	// TODO - check for more file errors.
	// TODO - add some checksum in order to ensure file contents is valid?

	ChunksStorage::Region result_region;

	for(uint32_t i= 0; i < c_world_region_area; ++i)
	{
		const RegionFile::ChunkHeader& chunk_header= file_header.chunks[i];
		if(chunk_header.block_data_size != 0 && chunk_header.auxiliar_data_size != 0)
		{
			ChunkDataCompresed& chunk_data= result_region.chunks[i];

			chunk_data.blocks.resize(chunk_header.block_data_size);
			file.seekg(chunk_header.block_data_offset);
			file.read(chunk_data.blocks.data(), chunk_header.block_data_size);

			chunk_data.auxiliar_data.resize(chunk_header.auxiliar_data_size);
			file.seekg(chunk_header.auxiliar_data_offset);
			file.read(chunk_data.auxiliar_data.data(), chunk_header.auxiliar_data_size);
		}
	}

	return result_region;
}

ChunksStorage::Region ChunksStorage::LoadOrCreateNewRegion(const std::string& file_name)
{
	auto region_opt= LoadRegion(file_name);
	if(region_opt != std::nullopt)
		return std::move(*region_opt);

	return Region();
}

ChunkDataCompresed& ChunksStorage::GetChunkData(const ChunkCoord chunk_coord)
{
	const RegionCoord region_coord= GetRegionCoordForChunk(chunk_coord);
	Region& region= EnsureRegionLoaded(region_coord);

	const int32_t coord_within_region[]
	{
		chunk_coord[0] - region_coord[0],
		chunk_coord[1] - region_coord[1],
	};
	HEX_ASSERT(coord_within_region[0] >= 0 && coord_within_region[0] < int32_t(c_world_region_size[0]));
	HEX_ASSERT(coord_within_region[1] >= 0 && coord_within_region[1] < int32_t(c_world_region_size[1]));

	const uint32_t chunk_index=
		uint32_t(coord_within_region[0]) + uint32_t(coord_within_region[1]) * c_world_region_size[0];

	return region.chunks[chunk_index];
}

ChunksStorage::Region& ChunksStorage::EnsureRegionLoaded(const RegionCoord region_coord)
{
	if(regions_map_.count(region_coord) == 0)
	{
		// If we have no such region and regions loading task is running - wait for finish.
		// Normally this should not happen, because regions loading should be started in advance.
		EnsureRegionsLoadingTaskFinished();
	}

	if(const auto it= regions_map_.find(region_coord); it != regions_map_.end())
	{
		// Already has this region.
		return it->second;
	}

	// Fallback - synchronously load the region or create new.
	return regions_map_.emplace(region_coord, LoadOrCreateNewRegion(GetRegionFilePath(region_coord))).first->second;
}

std::string ChunksStorage::GetRegionFilePath(const RegionCoord region_coord) const
{
	std::string res;
	res+= world_dir_path_;
	res+= "/";
	res+= "lon_";
	res+= std::to_string(region_coord[0]);
	res+= "_lat_";
	res+= std::to_string(region_coord[1]);
	res+= ".region";
	return res;
}

void ChunksStorage::TakeRegionsLoadingTaskResultIfReady()
{
	if(!regions_loading_future_.valid())
		return;

	const std::future_status status= regions_loading_future_.wait_for(std::chrono::milliseconds(0));
	if(status != std::future_status::ready)
		return;

	PopulateRegionsMap(regions_loading_future_.get());

	// Reset future to indicate it's unactive.
	regions_loading_future_= RegionsLoadingFuture();
	HEX_ASSERT(!regions_loading_future_.valid());
}

void ChunksStorage::EnsureRegionsLoadingTaskFinished()
{
	if(!regions_loading_future_.valid())
		return;

	regions_loading_future_.wait();

	PopulateRegionsMap(regions_loading_future_.get());

	// Reset future to indicate it's unactive.
	regions_loading_future_= RegionsLoadingFuture();
	HEX_ASSERT(!regions_loading_future_.valid());
}

void ChunksStorage::PopulateRegionsMap(const LoadedRegionsListPtr loaded_regions)
{
	HEX_ASSERT(loaded_regions != nullptr);

	for(auto& loaded_region : *loaded_regions)
	{
		HEX_ASSERT(regions_map_.count(loaded_region.first) == 0);
		regions_map_.emplace(loaded_region.first, std::move(loaded_region.second));
	}
}

} // namespace HexGPU
