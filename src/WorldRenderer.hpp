#pragma once
#include "Mat.hpp"
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldRenderer
{
public:
	WorldRenderer(WindowVulkan& window_vulkan);

	~WorldRenderer();

	void Draw(vk::CommandBuffer command_buffer, const m_Mat4& view_matrix);

private:
	const vk::Device vk_device_;
	const vk::Extent2D viewport_size_;
	const vk::RenderPass vk_render_pass_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;
	vk::UniqueDescriptorSetLayout vk_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_pipeline_layout_;
	vk::UniquePipeline vk_pipeline_;

	vk::UniqueDescriptorPool vk_descriptor_pool_;
	vk::UniqueDescriptorSet vk_descriptor_set_;

	vk::UniqueBuffer vk_vertex_buffer_;
	vk::UniqueDeviceMemory vk_vertex_buffer_memory_;

	vk::UniqueBuffer vk_index_buffer_;
	vk::UniqueDeviceMemory vk_index_buffer_memory_;

	size_t num_quads_= 0;
};

} // namespace HexGPU
