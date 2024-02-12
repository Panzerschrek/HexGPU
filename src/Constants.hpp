#pragma once
#include <cstdint>

namespace HexGPU
{

const uint32_t c_chunk_width_log2= 4;
const uint32_t c_chunk_width= 1 << c_chunk_width_log2;

const uint32_t c_chunk_height_log2= 7;
const uint32_t c_chunk_height= 1 << c_chunk_height_log2;

// In blocks.
const uint32_t c_chunk_volume= c_chunk_width * c_chunk_width * c_chunk_height;

inline uint32_t ChunkBlockAddress(const uint32_t x, const uint32_t y, const uint32_t z)
{
	// TODO - add range assert.
	return z + (y << c_chunk_height_log2) + (x << (c_chunk_width_log2 + c_chunk_height_log2));
}

} // namespace HexGPU
