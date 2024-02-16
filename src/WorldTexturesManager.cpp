#include "WorldTexturesManager.hpp"
#include "Assert.hpp"
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

} // namespace

WorldTexturesManager::WorldTexturesManager(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
{
	const auto image= LoadImage("textures/brick.jpg");
	HEX_ASSERT(image != std::nullopt);

	(void) FillTestImage;

	const uint32_t c_num_mips= 1; // TODO - create mips.
	const uint32_t num_layers= 1;

	image_= vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::Extent3D(image->size[0], image->size[1], 1u),
			c_num_mips,
			num_layers,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eLinear, // TODO - use optimal
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			vk::SharingMode::eExclusive,
			0u, nullptr,
			vk::ImageLayout::ePreinitialized));


	const vk::MemoryRequirements memory_requirements= vk_device_.getImageMemoryRequirements(*image_);

	const vk::PhysicalDeviceMemoryProperties memory_properties= window_vulkan.GetMemoryProperties();

	vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
	for(uint32_t j= 0u; j < memory_properties.memoryTypeCount; ++j)
	{
		if((memory_requirements.memoryTypeBits & (1u << j)) != 0 &&
			(memory_properties.memoryTypes[j].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
			memory_allocate_info.memoryTypeIndex= j;
	}

	image_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
	vk_device_.bindImageMemory(*image_, *image_memory_, 0u);

	void* image_data_gpu_size= nullptr;
	vk_device_.mapMemory(*image_memory_, 0u, memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &image_data_gpu_size);
	std::memcpy(image_data_gpu_size, image->data.data(), image->size[0] * image->size[1] * sizeof(PixelType));
	vk_device_.unmapMemory(*image_memory_);

	image_view_= vk_device_.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			*image_,
			vk::ImageViewType::e2DArray,
			vk::Format::eR8G8B8A8Unorm,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, 0u, num_layers)));
}

WorldTexturesManager::~WorldTexturesManager()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

vk::ImageView WorldTexturesManager::GetImageView() const
{
	return image_view_.get();
}

} // namespace HexGPU
