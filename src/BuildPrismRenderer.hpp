#pragma once
#include "WorldProcessor.hpp"
#include "Mat.hpp"

namespace HexGPU
{

class BuildPrismRenderer
{
public:
	BuildPrismRenderer(WindowVulkan& window_vulkan, WorldProcessor& world_processor);

	~BuildPrismRenderer();

	void PrepareFrame(vk::CommandBuffer command_buffer);
	void Draw(vk::CommandBuffer command_buffer, const m_Mat4& view_matrix);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	WorldProcessor& world_processor_;

	vk::UniqueBuffer uniform_buffer_;
	vk::UniqueDeviceMemory uniform_buffer_memory_;

	uint32_t vertex_buffer_num_vertices_= 0;
	vk::UniqueBuffer vertex_buffer_;
	vk::UniqueDeviceMemory vertex_buffer_memory_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;

	vk::UniqueDescriptorSetLayout decriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;

	vk::UniqueDescriptorPool descriptor_pool_;
	vk::UniqueDescriptorSet descriptor_set_;
};

} // namespace HexGPU
