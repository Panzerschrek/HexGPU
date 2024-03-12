#pragma once
#include "Image.hpp"

namespace HexGPU
{

void WriteTGA(
	uint16_t width, uint16_t height,
	const PixelType* const data,
	const char* const file_path);

} // namespace HexGPU
