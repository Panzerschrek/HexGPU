#pragma once
#include <cstdint>

namespace HexGPU
{

constexpr uint32_t c_chunk_width_log2= 4;
constexpr uint32_t c_chunk_width= 1 << c_chunk_width_log2;

constexpr uint32_t c_chunk_height_log2= 7;
constexpr uint32_t c_chunk_height= 1 << c_chunk_height_log2;

// In blocks.
constexpr uint32_t c_chunk_volume= c_chunk_width * c_chunk_width * c_chunk_height;

} // namespace HexGPU
