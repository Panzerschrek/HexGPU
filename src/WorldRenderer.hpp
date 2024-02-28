#pragma once
#include "Mat.hpp"
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
	void Draw(vk::CommandBuffer command_buffer, const m_Mat4& view_matrix);

private:
	void BuildDrawIndirectBuffer(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const WorldSizeChunks world_size_;

	WorldGeometryGenerator geometry_generator_;
	WorldTexturesManager world_textures_manager_;

	vk::UniqueBuffer draw_indirect_buffer_;
	vk::UniqueDeviceMemory draw_indirect_buffer_memory_;

	vk::UniqueShaderModule draw_indirect_buffer_build_shader_;
	vk::UniqueDescriptorSetLayout draw_indirect_buffer_build_decriptor_set_layout_;
	vk::UniquePipelineLayout draw_indirect_buffer_build_pipeline_layout_;
	vk::UniquePipeline draw_indirect_buffer_build_pipeline_;
	vk::DescriptorSet draw_indirect_buffer_build_descriptor_set_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;
	vk::UniqueSampler texture_sampler_;
	vk::UniqueDescriptorSetLayout decriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;
	vk::DescriptorSet descriptor_set_;

	vk::UniqueBuffer index_buffer_;
	vk::UniqueDeviceMemory index_buffer_memory_;
};

} // namespace HexGPU
