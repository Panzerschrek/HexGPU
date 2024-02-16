#include "WorldTexturesManager.hpp"
#include "Assert.hpp"
#include "Log.hpp"
#include <optional>
#include <vector>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

// This is single place, where this library included.
// So, we can include implementation here.
#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace HexGPU
{

namespace
{

using PixelType= uint32_t;

void FillTestImage(uint32_t width, uint32_t height, PixelType* const data)
{
	for(uint32_t y= 0; y < height; ++y)
	for(uint32_t x= 0; x < width; ++x)
	{
		data[x + y * width]= (x * 255 / width) | ((y * 255 / height) << 8);
	}
}

struct Image
{
	uint32_t size[2];
	std::vector<PixelType> data;
};

std::optional<Image> LoadImage(const char* const file_path)
{
	const std::string file_path_str(file_path);

	int width, height, channels;
	unsigned char* stbi_img_data= stbi_load(file_path_str.c_str(), &width, &height, &channels, 0);
	if(stbi_img_data == nullptr)
		return std::nullopt;

	if(channels == 4)
	{
		const auto data_ptr= reinterpret_cast<const PixelType*>(stbi_img_data);

		Image result;
		result.size[0]= uint32_t(width);
		result.size[1]= uint32_t(height);
		result.data= std::vector<PixelType>(data_ptr, data_ptr + width * height);
		stbi_image_free(stbi_img_data);
		return result;
	}
	if( channels == 3)
	{

		Image result;
		result.size[0]= uint32_t(width);
		result.size[1]= uint32_t(height);
		result.data.resize(width * height);

		for(int i= 0; i < width * height; ++i)
			result.data[i]=
				(stbi_img_data[i*3  ]<< 0) |
				(stbi_img_data[i*3+1]<< 8) |
				(stbi_img_data[i*3+2]<<16) |
				(255                << 24);

		return result;
	}

	stbi_image_free(stbi_img_data);
	return std::nullopt;
}

const uint32_t c_texture_size_log2= 8;
const uint32_t c_texture_size= 1 << c_texture_size_log2;

const char* const file_names[]=
{
	"textures/brick.jpg",
	"textures/stone.jpg",
	"textures/wood.jpg",
	"textures/soil.jpg",
};

const uint32_t c_num_mips= 1; // TODO - create mips.
const uint32_t num_layers= std::size(file_names);

} // namespace

WorldTexturesManager::WorldTexturesManager(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, vk_queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, memory_properties_(window_vulkan.GetMemoryProperties())
{
	image_= vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::Extent3D(c_texture_size, c_texture_size, 1u),
			c_num_mips,
			num_layers,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			vk::SharingMode::eExclusive,
			0u, nullptr,
			vk::ImageLayout::eGeneral));

	// Allocate memory.

	{
		const vk::MemoryRequirements memory_requirements= vk_device_.getImageMemoryRequirements(*image_);

		vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
		for(uint32_t j= 0u; j < memory_properties_.memoryTypeCount; ++j)
		{
			if((memory_requirements.memoryTypeBits & (1u << j)) != 0 &&
				(memory_properties_.memoryTypes[j].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				memory_allocate_info.memoryTypeIndex= j;
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
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, 0u, num_layers)));

	// Create staging buffer.
	// For now create it with size of whole texture.
	const uint32_t buffer_data_size= c_texture_size * c_texture_size * num_layers * sizeof(PixelType) * 2;
	{

		staging_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					buffer_data_size,
					vk::BufferUsageFlagBits::eTransferSrc));

		const vk::MemoryRequirements memory_requirements= vk_device_.getBufferMemoryRequirements(*staging_buffer_);

		vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
		{
			if((memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible ) != vk::MemoryPropertyFlags() &&
				(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				memory_allocate_info.memoryTypeIndex= i;
		}

		staging_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*staging_buffer_, *staging_buffer_memory_, 0u);
	}

	void* staging_buffer_mapped= nullptr;
	vk_device_.mapMemory(*staging_buffer_memory_, 0u, buffer_data_size, vk::MemoryMapFlags(), &staging_buffer_mapped);

	// Load files.

	// For now just fill data into the staging buffer.

	uint32_t dst_image_index= 0;
	for(const char* const file_name : file_names)
	{
		(void)FillTestImage;

		const auto image= LoadImage(file_name);
		if(image == std::nullopt)
		{
			Log::Warning("Can't load image ", file_name);
			continue;
		}
		if(image->size[0] != c_texture_size || image->size[1] != c_texture_size)
		{
			Log::Warning("Invalid size of image ", file_name);
			continue;
		}

		std::memcpy(
			static_cast<PixelType*>(staging_buffer_mapped) + dst_image_index * c_texture_size * c_texture_size,
			image->data.data(),
			c_texture_size * c_texture_size * sizeof(PixelType));

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
	// Copy buffer into the image, because we need a command buffer.

	if(textures_loaded_)
		return;
	textures_loaded_= true;

	for(uint32_t dst_image_index= 0; dst_image_index < num_layers; ++dst_image_index)
	{
		const vk::BufferImageCopy copy_region(
			dst_image_index * c_texture_size * c_texture_size * sizeof(PixelType),
			c_texture_size, c_texture_size,
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0u, dst_image_index, 1u),
			vk::Offset3D(0, 0, 0),
			vk::Extent3D(c_texture_size, c_texture_size, 1u));

		command_buffer.copyBufferToImage(
			*staging_buffer_,
			*image_,
			vk::ImageLayout::eGeneral,
			1u, &copy_region);
	}

	// Wait for update.
	// TODO - check if this is correct.
	vk::BufferMemoryBarrier barrier;
	barrier.srcAccessMask= vk::AccessFlagBits::eTransferRead;
	barrier.dstAccessMask= vk::AccessFlagBits::eTransferWrite;
	barrier.size= VK_WHOLE_SIZE;
	barrier.buffer= *staging_buffer_;
	barrier.srcQueueFamilyIndex= vk_queue_family_index_;
	barrier.dstQueueFamilyIndex= vk_queue_family_index_;

	command_buffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eTransfer,
		vk::DependencyFlags(),
		0, nullptr,
		1, &barrier,
		0, nullptr);

	// TODO - switch layout to eShaderReadOnlyOptimal.
}

vk::ImageView WorldTexturesManager::GetImageView() const
{
	return image_view_.get();
}

} // namespace HexGPU
