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
		vk::MemoryPropertyFlags memory_visibility);

	vk::DeviceSize GetSize() const;

	vk::Buffer GetBuffer() const;

private:
	const vk::DeviceSize size_;
	vk::UniqueBuffer buffer_;
	vk::UniqueDeviceMemory buffer_memory_;
};

} // namespace HexGPU
