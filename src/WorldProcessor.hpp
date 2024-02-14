#pragma once
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldProcessor
{
public:
	WorldProcessor(WindowVulkan& window_vulkan);
	~WorldProcessor();

	vk::Buffer GetChunkDataBuffer() const;
	uint32_t GetChunkDataBufferSize() const;

private:
	const vk::Device vk_device_;
	const uint32_t vk_queue_family_index_;

	vk::UniqueBuffer vk_chunk_data_buffer_;
	vk::UniqueDeviceMemory vk_chunk_data_buffer_memory_;
	uint32_t chunk_data_buffer_size_= 0;
};

} // namespace HexGPU
