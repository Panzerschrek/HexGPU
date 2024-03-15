#include "ChunksStorage.hpp"

namespace HexGPU
{

void ChunksStorage::SetChunk(const ChunkCoord chunk_coord, ChunkDataCompresed data_compressed)
{
	chunks_map_[chunk_coord]= std::move(data_compressed);
}

const ChunkDataCompresed* ChunksStorage::GetChunk(const ChunkCoord coord)
{
	const auto it= chunks_map_.find(coord);
	if(it == chunks_map_.end())
		return nullptr;

	return &it->second;
}

bool ChunksStorage::HasDataForChunk(const ChunkCoord coord)
{
	return chunks_map_.find(coord) != chunks_map_.end();
}

} // namespace HexGPU
