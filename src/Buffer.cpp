#include "Buffer.hpp"

namespace HexGPU
{

Buffer::Buffer(
	WindowVulkan& window_vulkan,
	const vk::DeviceSize size,
	const vk::BufferUsageFlags usage_flags,
	const vk::MemoryPropertyFlags memory_visibility)
	: size_(size)
{
	const auto vk_device= window_vulkan.GetVulkanDevice();

	buffer_= vk_device.createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(), size_, usage_flags));

	const vk::MemoryRequirements buffer_memory_requirements= vk_device.getBufferMemoryRequirements(*buffer_);

	const auto memory_properties= window_vulkan.GetMemoryProperties();

	vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
	for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
	{
		if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & memory_visibility) == memory_visibility)
		{
			memory_allocate_info.memoryTypeIndex= i;
			break;
		}
	}

	buffer_memory_= vk_device.allocateMemoryUnique(memory_allocate_info);
	vk_device.bindBufferMemory(*buffer_, *buffer_memory_, 0u);
}

vk::DeviceSize Buffer::GetSize() const
{
	return size_;
}

vk::Buffer Buffer::GetBuffer() const
{
	return *buffer_;
}

void* Buffer::Map(const vk::Device vk_device) const
{
	void* ptr_mapped= nullptr;
	vk_device.mapMemory(*buffer_memory_, 0u, size_, vk::MemoryMapFlags(), &ptr_mapped);
	return ptr_mapped;
}

void Buffer::Unmap(const vk::Device vk_device) const
{
	vk_device.unmapMemory(*buffer_memory_);
}

} // namespace HexGPU
