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
	const uint32_t vk_queue_family_index_;
	WorldProcessor& world_processor_;

	uint32_t vertex_buffer_num_vertices_= 0;
	vk::UniqueBuffer vk_vertex_buffer_;
	vk::UniqueDeviceMemory vk_vertex_buffer_memory_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;

	vk::UniqueDescriptorSetLayout vk_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_pipeline_layout_;
	vk::UniquePipeline vk_pipeline_;

	vk::UniqueDescriptorPool vk_descriptor_pool_;
	vk::UniqueDescriptorSet vk_descriptor_set_;
};

} // namespace HexGPU
