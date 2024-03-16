#pragma once
#include <cstdint>

namespace HexGPU
{

// Size of region in chunks.
// This is'nt square because chunks aren't square and we try to keep real region size square.
constexpr uint32_t c_world_region_size[2]{14, 12};
constexpr uint32_t c_world_region_area= c_world_region_size[0] * c_world_region_size[1];

namespace RegionFile
{

struct ChunkHeader
{
	uint32_t block_data_offset= 0;
	uint32_t block_data_size= 0;
	uint32_t auxiliar_data_offset= 0;
	uint32_t auxiliar_data_size= 0;
};

struct FileHeader
{
	char id[16]{};
	uint64_t version= 0;

	// All chunks of this region.
	// If size is zero - there is no data for given chunk
	ChunkHeader chunks[c_world_region_area];
};

constexpr char c_expected_id[]= "HexRegio";
constexpr uint64_t c_expected_version= 31; // Change this each time format is changed.

} // namespace RegionFile

} // namespace HexGPU
