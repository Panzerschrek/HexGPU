#include "StructuresBuffer.hpp"

namespace HexGPU
{

StructuresBuffer::StructuresBuffer(
	WindowVulkan& window_vulkan,
	GPUDataUploader& gpu_data_uploader,
	const Structures& structures)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, description_buffer_(
		window_vulkan,
		sizeof(StructureDescription) * structures.descriptions.size(),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, data_buffer_(
		window_vulkan,
		sizeof(BlockType) * structures.data.size(),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
{
	gpu_data_uploader.UploadData(
		structures.descriptions.data(),
		sizeof(StructureDescription) * structures.descriptions.size(),
		description_buffer_.GetBuffer(),
		0);

	gpu_data_uploader.UploadData(
		structures.data.data(),
		sizeof(BlockType) * structures.data.size(),
		data_buffer_.GetBuffer(),
		0);
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
