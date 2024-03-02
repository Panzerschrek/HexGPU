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
		WorldProcessor& world_processor,
		vk::DescriptorPool global_descriptor_pool);

	~BuildPrismRenderer();

	void PrepareFrame(vk::CommandBuffer command_buffer);
	void Draw(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	WorldProcessor& world_processor_;

	vk::UniqueBuffer uniform_buffer_;
	vk::UniqueDeviceMemory uniform_buffer_memory_;

	uint32_t vertex_buffer_num_vertices_= 0;
	vk::UniqueBuffer vertex_buffer_;
	vk::UniqueDeviceMemory vertex_buffer_memory_;

	GraphicsPipeline pipeline_;
	vk::DescriptorSet descriptor_set_;
};

} // namespace HexGPU
