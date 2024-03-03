#pragma once
#include "Pipeline.hpp"
#include "WorldProcessor.hpp"

namespace HexGPU
{

class BuildPrismRenderer
{
public:
	BuildPrismRenderer(
		WindowVulkan& window_vulkan,
		const WorldProcessor& world_processor,
		vk::DescriptorPool global_descriptor_pool);

	~BuildPrismRenderer();

	void PrepareFrame(vk::CommandBuffer command_buffer);
	void Draw(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	const WorldProcessor& world_processor_;

	const Buffer uniform_buffer_;

	uint32_t vertex_buffer_num_vertices_= 0;
	vk::UniqueBuffer vertex_buffer_;
	vk::UniqueDeviceMemory vertex_buffer_memory_;

	const GraphicsPipeline pipeline_;
	const vk::DescriptorSet descriptor_set_;
};

} // namespace HexGPU
