#pragma once
#include "Pipeline.hpp"
#include "WorldGeometryGenerator.hpp"
#include "WorldTexturesManager.hpp"

namespace HexGPU
{

class WorldRenderer
{
public:
	WorldRenderer(
		WindowVulkan& window_vulkan,
		WorldProcessor& world_processor,
		vk::DescriptorPool global_descriptor_pool);

	~WorldRenderer();

	void PrepareFrame(vk::CommandBuffer command_buffer);
	void Draw(vk::CommandBuffer command_buffer);

private:
	void BuildDrawIndirectBuffer(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	const WorldProcessor& world_processor_;

	const WorldSizeChunks world_size_;

	WorldGeometryGenerator geometry_generator_;
	WorldTexturesManager world_textures_manager_;

	vk::UniqueBuffer draw_indirect_buffer_;
	vk::UniqueDeviceMemory draw_indirect_buffer_memory_;

	vk::UniqueBuffer uniform_buffer_;
	vk::UniqueDeviceMemory uniform_buffer_memory_;

	ComputePipeline draw_indirect_buffer_build_pipeline_;
	vk::DescriptorSet draw_indirect_buffer_build_descriptor_set_;

	vk::UniqueSampler texture_sampler_;
	GraphicsPipeline draw_pipeline_;
	vk::DescriptorSet descriptor_set_;

	vk::UniqueBuffer index_buffer_;
	vk::UniqueDeviceMemory index_buffer_memory_;
};

} // namespace HexGPU
