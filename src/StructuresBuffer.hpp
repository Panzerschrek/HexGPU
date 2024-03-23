#pragma once
#include "GPUDataUploader.hpp"
#include "Structures.hpp"

namespace HexGPU
{

class StructuresBuffer
{
public:
	StructuresBuffer(WindowVulkan& window_vulkan, GPUDataUploader& gpu_data_uploader, const Structures& structures);
	~StructuresBuffer();

	vk::Buffer GetDescriptionsBuffer() const;
	vk::DeviceSize GetDescriptionsBufferSize() const;

	vk::Buffer GetDataBuffer() const;
	vk::DeviceSize GetDataBufferSize() const;

private:
	const vk::Device vk_device_;

	const Buffer description_buffer_;
	const Buffer data_buffer_;
};

} // namespace HexGPU
