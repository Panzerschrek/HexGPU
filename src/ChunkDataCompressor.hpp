#pragma once
#include "BlockType.hpp"
#include <string>

namespace HexGPU
{

struct ChunkDataCompresed
{
	std::string blocks;
	std::string auxiliar_data;
};

class ChunkDataCompressor
{
public:
	// Input arrays are both of "c_chunk_volume" size.
	ChunkDataCompresed Compress(const BlockType* blocks_data, const uint8_t* blocks_auxiliar_data);

	// Fills provided buffers of "c_chunk_volume" size.
	// Returns true on success.
	bool Decompress(const ChunkDataCompresed& data_compressed, BlockType* blocks_data, uint8_t* blocks_auxiliar_data);

private:
	std::string temp_compress_buffer_;
};

} // namespace HexGPU
