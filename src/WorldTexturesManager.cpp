#include "WorldTexturesManager.hpp"
#include "TexturesTable.hpp"
#include "Log.hpp"

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

namespace
{

using PixelType= uint32_t;

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

const uint32_t c_texture_size_log2= 8;
const uint32_t c_texture_size= 1 << c_texture_size_log2;

const uint32_t c_num_mips= c_texture_size_log2 - 2; // Ignore last two mips for simplicity.

constexpr uint32_t c_num_layers= uint32_t(std::size(c_block_textures_table));

// Add extra padding (use 3/2 instead of 4/3).
const uint32_t c_texture_num_texels_with_mips= c_texture_size * c_texture_size * 3 / 2;

} // namespace

WorldTexturesManager::WorldTexturesManager(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, image_(vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::Extent3D(c_texture_size, c_texture_size, 1u),
			c_num_mips,
			c_num_layers,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			vk::SharingMode::eExclusive,
			0u, nullptr,
			vk::ImageLayout::eUndefined)))
{
	const auto memory_properties= window_vulkan.GetMemoryProperties();

	// Allocate image memory.
	{
		const vk::MemoryRequirements memory_requirements= vk_device_.getImageMemoryRequirements(*image_);

		vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
		for(uint32_t j= 0u; j < memory_properties.memoryTypeCount; ++j)
		{
			if((memory_requirements.memoryTypeBits & (1u << j)) != 0 &&
				(memory_properties.memoryTypes[j].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= j;
				break;
			}
		}

		image_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindImageMemory(*image_, *image_memory_, 0u);
	}

	// Create image view.
	image_view_= vk_device_.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			*image_,
			vk::ImageViewType::e2DArray,
			vk::Format::eR8G8B8A8Unorm,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, 0u, c_num_layers)));

	// Create staging buffer.
	// For now create it with size of whole texture (with mips and extra padding).
	const uint32_t buffer_data_size= c_num_layers * c_texture_num_texels_with_mips * sizeof(PixelType) * 2;
	{
		staging_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					buffer_data_size,
					vk::BufferUsageFlagBits::eTransferSrc));

		const vk::MemoryRequirements memory_requirements= vk_device_.getBufferMemoryRequirements(*staging_buffer_);

		vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible ) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= i;
				break;
			}
		}

		staging_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*staging_buffer_, *staging_buffer_memory_, 0u);
	}

	// Load files.
	// For now just fill data into the mapped staging buffer.

	void* staging_buffer_mapped= nullptr;
	vk_device_.mapMemory(*staging_buffer_memory_, 0u, buffer_data_size, vk::MemoryMapFlags(), &staging_buffer_mapped);

	uint32_t dst_image_index= 0;
	for(const char* const texture_file : c_block_textures_table)
	{
		PixelType* dst= static_cast<PixelType*>(staging_buffer_mapped) + dst_image_index * c_texture_num_texels_with_mips;
		LoadImageWithExpectedSize((std::string("textures/") + texture_file).c_str(), c_texture_size, c_texture_size, dst);

		for(uint32_t mip= 1; mip < c_num_mips; ++mip)
		{
			const uint32_t current_size= c_texture_size >> mip;
			const uint32_t prev_size= current_size << 1;

			PixelType* const mip_dst= dst + prev_size * prev_size;
			RGBA8_GetMip(reinterpret_cast<const uint8_t*>(dst), reinterpret_cast<uint8_t*>(mip_dst), prev_size, prev_size);

			dst= mip_dst;
		}

		++dst_image_index;
	}

	vk_device_.unmapMemory(*staging_buffer_memory_);
}

WorldTexturesManager::~WorldTexturesManager()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldTexturesManager::PrepareFrame(const vk::CommandBuffer command_buffer)
{
	// Copy buffer into the image after ininitialization, because we need a command buffer.

	if(textures_loaded_)
		return;
	textures_loaded_= true;

	// Transfer to dst optimal
	{
		const vk::ImageMemoryBarrier image_memory_transfer(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
			queue_family_index_, queue_family_index_,
			*image_,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, 0u, c_num_layers));

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags(),
			{},
			{},
			{image_memory_transfer});
	}

	for(uint32_t dst_image_index= 0; dst_image_index < c_num_layers; ++dst_image_index)
	{
		uint32_t offset= dst_image_index * c_texture_num_texels_with_mips * uint32_t(sizeof(PixelType));

		for(uint32_t mip= 0; mip < c_num_mips; ++mip)
		{
			const uint32_t current_size= c_texture_size >> mip;

			command_buffer.copyBufferToImage(
				*staging_buffer_,
				*image_,
				vk::ImageLayout::eTransferDstOptimal,
				{
					{
						offset,
						current_size, current_size,
						vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, mip, dst_image_index, 1u),
						vk::Offset3D(0, 0, 0),
						vk::Extent3D(current_size, current_size, 1u)
					}
				});

			offset+= current_size * current_size * uint32_t(sizeof(PixelType));
		}
	}

	// Wait for update and transfer layout.
	// TODO - check if this is correct.
	{
		const vk::ImageMemoryBarrier image_memory_transfer(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			queue_family_index_, queue_family_index_,
			*image_,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, 0u, c_num_layers));

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags(),
			0u, nullptr,
			0u, nullptr,
			1u, &image_memory_transfer);
	}
}

vk::ImageView WorldTexturesManager::GetImageView() const
{
	return image_view_.get();
}

} // namespace HexGPU
