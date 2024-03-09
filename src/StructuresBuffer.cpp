#include "StructuresBuffer.hpp"

namespace HexGPU
{

StructuresBuffer::StructuresBuffer(WindowVulkan& window_vulkan, const Structures& structures)
	: vk_device_(window_vulkan.GetVulkanDevice())
	// TODO - avoid making host visible.
	, description_buffer_(
		window_vulkan,
		sizeof(StructureDescription) * structures.descriptions.size(),
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
	, data_buffer_(
		window_vulkan,
		sizeof(BlockType) * structures.data.size(),
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
{
	void* const description_buffer_data= description_buffer_.Map(vk_device_);
	std::memcpy(
		description_buffer_data,
		structures.descriptions.data(),
		sizeof(StructureDescription) * structures.descriptions.size());
	description_buffer_.Unmap(vk_device_);

	void* const structures_buffer_data= data_buffer_.Map(vk_device_);
	std::memcpy(
		structures_buffer_data,
		structures.data.data(),
		sizeof(BlockType) * structures.data.size());
	data_buffer_.Unmap(vk_device_);
}

StructuresBuffer::~StructuresBuffer()
{
	vk_device_.waitIdle();
}

vk::Buffer StructuresBuffer::GetDescriptionsBuffer() const
{
	return description_buffer_.GetBuffer();
}

vk::DeviceSize StructuresBuffer::GetDescriptionsBufferSize() const
{
	return description_buffer_.GetSize();
}

vk::Buffer StructuresBuffer::GetDataBuffer() const
{
	return data_buffer_.GetBuffer();
}

vk::DeviceSize StructuresBuffer::GetDataBufferSize() const
{
	return data_buffer_.GetSize();
}

} // namespace HexGPU
