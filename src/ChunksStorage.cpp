#include "ChunksStorage.hpp"
#include "Log.hpp"
#include "Math.hpp"
#include <cstring>

namespace HexGPU
{

void ChunksStorage::SetChunk(const ChunkCoord chunk_coord, ChunkDataCompresed data_compressed)
{
	// TODO - load region if necessary.
	const RegionCoord region_coord= GetRegionCoordForChunk(chunk_coord);
	Region& region= regions_map_[region_coord];

	const int32_t coord_within_region[]
	{
		chunk_coord[0] - region_coord[0],
		chunk_coord[1] - region_coord[1],
	};
	HEX_ASSERT(coord_within_region[0] >= 0 && coord_within_region[0] < int32_t(c_world_region_size[0]));
	HEX_ASSERT(coord_within_region[1] >= 0 && coord_within_region[1] < int32_t(c_world_region_size[1]));

	const uint32_t chunk_index=
		uint32_t(coord_within_region[0]) + uint32_t(coord_within_region[1]) * c_world_region_size[0];

	region.chunks[chunk_index]= std::move(data_compressed);
}

const ChunkDataCompresed* ChunksStorage::GetChunk(const ChunkCoord chunk_coord) const
{
	const RegionCoord region_coord= GetRegionCoordForChunk(chunk_coord);
	const auto it= regions_map_.find(region_coord);
	if(it == regions_map_.end())
		return nullptr;

	const Region& region= it->second;

	const int32_t coord_within_region[]
	{
		chunk_coord[0] - region_coord[0],
		chunk_coord[1] - region_coord[1],
	};
	HEX_ASSERT(coord_within_region[0] >= 0 && coord_within_region[0] < int32_t(c_world_region_size[0]));
	HEX_ASSERT(coord_within_region[1] >= 0 && coord_within_region[1] < int32_t(c_world_region_size[1]));

	const uint32_t chunk_index=
		uint32_t(coord_within_region[0]) + uint32_t(coord_within_region[1]) * c_world_region_size[0];

	const ChunkDataCompresed& chunk_data= region.chunks[chunk_index];
	if(!chunk_data.blocks.empty() && !chunk_data.auxiliar_data.empty())
		return &chunk_data;

	return nullptr;
}

bool ChunksStorage::HasDataForChunk(const ChunkCoord chunk_coord)
{
	return GetChunk(chunk_coord) != nullptr;
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
		Log::Info("Can't open file \"", file_name, "\"");
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
		Log::Info("Failed to flush region file!");
		return false;
	}

	return true;
}

std::optional<ChunksStorage::Region> ChunksStorage::LoadRegion(const std::string& file_name)
{
	std::ifstream file(file_name, std::ios::binary);
	if(!file.is_open())
	{
		Log::Info("Can't open file \"", file_name, "\"");
		return std::nullopt;
	}

	RegionFile::FileHeader file_header;
	file.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));

	if(std::memcmp(file_header.id, RegionFile::c_expected_id, sizeof(RegionFile::c_expected_id)) != 0)
	{
		Log::Info("File \"", file_name, "\" seems to be not a valid region file");
		return std::nullopt;
	}

	if(file_header.version != RegionFile::c_expected_version)
	{
		Log::Info("Region file version mismatch, expected ", RegionFile::c_expected_version, ", got ", file_header.version);
		return std::nullopt;
	}

	// TODO - check for more file errors.
	// TODO - add some checksum in order to ensure file contents is valid?

	ChunksStorage::Region result_region;

	for(uint32_t i= 0; i < c_world_region_area; ++i)
	{
		const RegionFile::ChunkHeader& chunk_header= file_header.chunks[i];
		if(chunk_header.block_data_size != 0 && chunk_header.block_data_size != 0)
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

} // namespace HexGPU
