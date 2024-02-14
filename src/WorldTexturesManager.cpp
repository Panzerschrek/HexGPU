#include "WorldTexturesManager.hpp"

namespace HexGPU
{

namespace
{

void FillTestImage(uint32_t width, uint32_t height, uint32_t* const data)
{
	for(uint32_t y= 0; y < height; ++y)
	for(uint32_t x= 0; x < width; ++x)
	{
		data[x + y * width]= (x * 255 / width) | ((y * 255 / height) << 8);
	}
}

} // namespace

WorldTexturesManager::WorldTexturesManager(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
{
	const uint32_t c_texture_size_log2= 8;
	const uint32_t c_texture_size= 1 << c_texture_size_log2;
	const uint32_t c_num_mips= 1; // TODO - create mips.

	image_= vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::Extent3D(c_texture_size, c_texture_size, 1u),
			c_num_mips,
			1u,
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
	FillTestImage(c_texture_size, c_texture_size, reinterpret_cast<uint32_t*>(image_data_gpu_size));
	vk_device_.unmapMemory(*image_memory_);

	image_view_= vk_device_.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			*image_,
			vk::ImageViewType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, 0u, 1u)));
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
