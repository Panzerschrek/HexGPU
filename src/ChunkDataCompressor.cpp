#include "ChunkDataCompressor.hpp"
#include "Assert.hpp"
#include "Constants.hpp"
#include <snappy.h>
#include <cstring>

namespace HexGPU
{

ChunkDataCompresed ChunkDataCompressor::Compress(
	const BlockType* const blocks_data,
	const uint8_t* const blocks_auxiliar_data)
{
	ChunkDataCompresed out_data;

	// TODO - check for correctness/possible errors.
	// TODO - tune compression params.
	snappy::Compress(reinterpret_cast<const char*>(blocks_data), c_chunk_volume, &out_data.blocks);
	snappy::Compress(reinterpret_cast<const char*>(blocks_auxiliar_data), c_chunk_volume, &out_data.auxiliar_data);

	return out_data;
}

bool ChunkDataCompressor::Decompress(
	const ChunkDataCompresed& data_compressed,
	BlockType* const blocks_data,
	uint8_t* const blocks_auxiliar_data)
{
	// TODO - check for errors.

	std::string uncompressed;

	// TODO - avoid using intermediate string.
	snappy::Uncompress(data_compressed.blocks.data(), data_compressed.blocks.size(), &uncompressed);
	HEX_ASSERT(uncompressed.size() == c_chunk_volume);
	std::memcpy(blocks_data, uncompressed.data(), c_chunk_volume);

	uncompressed.clear();
	snappy::Uncompress(data_compressed.auxiliar_data.data(), data_compressed.auxiliar_data.size(), &uncompressed);
	HEX_ASSERT(uncompressed.size() == c_chunk_volume);
	std::memcpy(blocks_auxiliar_data, uncompressed.data(), c_chunk_volume);

	return true;
}

} // namespace HexGPU
