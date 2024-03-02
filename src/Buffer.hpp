#pragma once
#include "WindowVulkan.hpp"

namespace HexGPU
{

class Buffer
{
public:
	Buffer(
		WindowVulkan& window_vulkan,
		vk::DeviceSize size,
		vk::BufferUsageFlags usage_flags,
		// Default visibility is device local only. It is fine for most cases.
		vk::MemoryPropertyFlags memory_visibility= vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::DeviceSize GetSize() const;

	vk::Buffer GetBuffer() const;

private:
	const vk::DeviceSize size_;
	vk::UniqueBuffer buffer_;
	vk::UniqueDeviceMemory buffer_memory_;
};

} // namespace HexGPU
