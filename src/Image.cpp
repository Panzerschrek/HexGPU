#include "Image.hpp"
#include "Log.hpp"
#include <cstring>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

// This is single place, where this library included.
// So, we can include implementation here.
// Limit supported image formats (leave only necessary).
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_ONLY_PNG
#include "../stb/stb_image.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace HexGPU
{

bool LoadImageWithExpectedSize(
	const char* const file_path,
	const uint32_t expected_width,
	const uint32_t expected_height,
	PixelType* const dst_pixels)
{
	const std::string file_path_str(file_path);

	int width= 0, height= 0, channels= 0;
	uint8_t* const stbi_img_data= stbi_load(file_path_str.c_str(), &width, &height, &channels, 0);
	if(stbi_img_data == nullptr)
	{
		Log::Warning("Can't load image ", file_path);
		return false;
	}

	if(width != int(expected_width) || height != int(expected_height))
	{
		Log::Warning("Wrong image ", file_path, " size");
		return false;
	}

	if(channels == 4)
		std::memcpy(dst_pixels, stbi_img_data, expected_width * expected_height * sizeof(PixelType));
	else if( channels == 3)
	{
		for(uint32_t i= 0; i < expected_width * expected_height; ++i)
			dst_pixels[i]=
				(stbi_img_data[i*3  ] <<  0) |
				(stbi_img_data[i*3+1] <<  8) |
				(stbi_img_data[i*3+2] << 16) |
				(255                  << 24);
	}
	else
		Log::Warning("Wrong number of image ", file_path, " channels: ", channels);

	stbi_image_free(stbi_img_data);
	return true;
}

void RGBA8_GetMip( const uint8_t* const in, uint8_t* const out, const uint32_t width, const uint32_t height)
{
	uint8_t* dst= out;
	for(uint32_t y= 0; y < height; y+= 2)
	{
		const uint8_t* src[2]= { in + y * width * 4, in + (y+1) * width * 4 };
		for(uint32_t x= 0; x < width; x+= 2, src[0]+= 8, src[1]+= 8, dst+= 4)
		{
			dst[0]= uint8_t((src[0][0] + src[0][4]  +  src[1][0] + src[1][4]) >> 2);
			dst[1]= uint8_t((src[0][1] + src[0][5]  +  src[1][1] + src[1][5]) >> 2);
			dst[2]= uint8_t((src[0][2] + src[0][6]  +  src[1][2] + src[1][6]) >> 2);
			dst[3]= uint8_t((src[0][3] + src[0][7]  +  src[1][3] + src[1][7]) >> 2);
		}
	}
}

} // namespace HexGPU
