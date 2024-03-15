#include "ChunksStorage.hpp"
#include <Assert.hpp>
#include "snappy.h"
#include <cstring>

namespace HexGPU
{

void ChunksStorage::SetChunk(
	const ChunkCoord chunk_coord,
	const BlockType* const blocks_data,
	const uint8_t* const blocks_auxiliar_data)
{
	ChunkDataCompresed& out_data= chunks_map_[chunk_coord];

	// TODO - check for correctness/possible errors.
	// TODO - tune compression params.
	snappy::Compress(reinterpret_cast<const char*>(blocks_data), c_chunk_volume, &out_data.blocks);
	snappy::Compress(reinterpret_cast<const char*>(blocks_auxiliar_data), c_chunk_volume, &out_data.auxiliar_data);
}

bool ChunksStorage::GetChunk(
	const ChunkCoord coord,
	BlockType* const blocks_data,
	uint8_t* const blocks_auxiliar_data)
{
	const auto it= chunks_map_.find(coord);
	if(it == chunks_map_.end())
		return false;

	const ChunkDataCompresed& in_data= it->second;

	std::string uncompressed;

	// TODO - avoid using intermediate string.
	snappy::Uncompress(in_data.blocks.data(), in_data.blocks.size(), &uncompressed);
	HEX_ASSERT(uncompressed.size() == c_chunk_volume);
	std::memcpy(blocks_data, uncompressed.data(), c_chunk_volume);

	uncompressed.clear();
	snappy::Uncompress(in_data.blocks.data(), in_data.auxiliar_data.size(), &uncompressed);
	HEX_ASSERT(uncompressed.size() == c_chunk_volume);
	std::memcpy(blocks_auxiliar_data, uncompressed.data(), c_chunk_volume);

	return true;
}

bool ChunksStorage::HasDataForChunk(const ChunkCoord coord)
{
	return chunks_map_.find(coord) != chunks_map_.end();
}

} // namespace HexGPU
