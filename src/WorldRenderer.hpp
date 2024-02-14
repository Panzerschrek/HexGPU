#pragma once
#include "Mat.hpp"
#include "WorldGeometryGenerator.hpp"

namespace HexGPU
{

class WorldRenderer
{
public:
	WorldRenderer(WindowVulkan& window_vulkan, WorldProcessor& world_processor);

	~WorldRenderer();

	void PrepareFrame(vk::CommandBuffer command_buffer);
	void Draw(vk::CommandBuffer command_buffer, const m_Mat4& view_matrix);

private:
	const vk::Device vk_device_;
	const uint32_t vk_queue_family_index_;

	WorldGeometryGenerator geometry_generator_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;

	vk::UniqueDescriptorSetLayout vk_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_pipeline_layout_;
	vk::UniquePipeline vk_pipeline_;

	vk::UniqueDescriptorPool vk_descriptor_pool_;
	vk::UniqueDescriptorSet vk_descriptor_set_;

	vk::UniqueBuffer vk_index_buffer_;
	vk::UniqueDeviceMemory vk_index_buffer_memory_;
};

} // namespace HexGPU
