#pragma once
#include <cstdint>

namespace HexGPU
{

// RGBA pixel.
using PixelType= uint32_t;

bool LoadImageWithExpectedSize(
	const char* file_path,
	uint32_t expected_width,
	uint32_t expected_height,
	PixelType* dst_pixels);

void RGBA8_GetMip(const uint8_t* in, uint8_t* out, uint32_t width, uint32_t height);

} // namespace HexGPU
