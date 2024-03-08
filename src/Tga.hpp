#pragma once
#include "Image.hpp"

namespace HexGPU
{

void WriteTGA(
	const uint16_t width, const uint16_t height,
	const PixelType* data,
	const char* file_path);

} // namespace HexGPU
