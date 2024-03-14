#include "ChunksStorage.hpp"
#include <Assert.hpp>
#include <cstring>

namespace HexGPU
{

void ChunksStorage::SetChunk(
	const ChunkCoord chunk_coord,
	const BlockType* const blocks_data,
	const uint8_t* const blocks_auxiliar_data)
{
	ChunkDataPtr& out_data= chunks_map_[chunk_coord];
	if(out_data == nullptr)
		out_data= std::make_unique<ChunkDataCombined>();

	std::memcpy(out_data->blocks, blocks_data, c_chunk_volume);
	std::memcpy(out_data->auxiliar_data, blocks_auxiliar_data, c_chunk_volume);
}

bool ChunksStorage::GetChunk(
	const ChunkCoord coord,
	BlockType* const blocks_data,
	uint8_t* const blocks_auxiliar_data)
{
	const auto it= chunks_map_.find(coord);
	if(it == chunks_map_.end())
		return false;

	const ChunkDataPtr& in_data= it->second;
	HEX_ASSERT(in_data != nullptr);

	std::memcpy(blocks_data, in_data->blocks, c_chunk_volume);
	std::memcpy(blocks_auxiliar_data, in_data->auxiliar_data, c_chunk_volume);

	return true;
}

} // namespace HexGPU
