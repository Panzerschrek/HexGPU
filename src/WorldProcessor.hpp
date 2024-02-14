#pragma once
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldProcessor
{
public:
	WorldProcessor(WindowVulkan& window_vulkan);
	~WorldProcessor();

	void PrepareFrame(vk::CommandBuffer command_buffer);

	vk::Buffer GetChunkDataBuffer() const;
	uint32_t GetChunkDataBufferSize() const;

private:
	const vk::Device vk_device_;
	const uint32_t vk_queue_family_index_;

	vk::UniqueBuffer vk_chunk_data_buffer_;
	vk::UniqueDeviceMemory vk_chunk_data_buffer_memory_;
	uint32_t chunk_data_buffer_size_= 0;

	vk::UniqueShaderModule world_gen_shader_;

	vk::UniqueDescriptorSetLayout vk_world_gen_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_world_gen_pipeline_layout_;
	vk::UniquePipeline vk_world_gen_pipeline_;
	vk::UniqueDescriptorPool vk_world_gen_descriptor_pool_;
	vk::UniqueDescriptorSet vk_world_gen_descriptor_set_;

	bool world_generated_= false;
};

} // namespace HexGPU
