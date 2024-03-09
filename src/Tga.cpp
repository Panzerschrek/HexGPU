#include "Image.hpp"
#include <fstream>

namespace HexGPU
{

#pragma pack(push, 1)
struct TGAHeader
{
	uint8_t id_length;
	uint8_t colormap_type;
	uint8_t image_type;

	uint16_t colormap_index;
	uint16_t colormap_length;
	uint8_t colormap_size;

	uint16_t x_origin;
	uint16_t y_origin;
	uint16_t width ;
	uint16_t height;

	uint8_t pixel_bits;
	uint8_t attributes;
};
#pragma pack(pop)

static_assert(sizeof(TGAHeader) == 18, "Invalid size");

void WriteTGA(
	const uint16_t width, const uint16_t height,
	const PixelType* const data,
	const char* const file_path)
{
	TGAHeader tga{};

	tga.id_length= 0;
	tga.colormap_type= 0; // image without colormap
	tga.image_type= 2; // incompressed true-color

	tga.colormap_index= 0;
	tga.colormap_length= 0;
	tga.colormap_size= 0;

	tga.x_origin= 0;
	tga.y_origin= 0;
	tga.width = width ;
	tga.height= height;

	tga.pixel_bits= 32;
	tga.attributes= 1 << 5; // vertical flip flag
	tga.attributes|= 8; // bits in alpha-channell

	std::ofstream file(file_path, std::ios::binary);
	file.write(reinterpret_cast<const char*>(&tga), sizeof(tga));
	file.write(reinterpret_cast<const char*>(data), width * height * sizeof(PixelType));
}

} // namespace HexGPU
