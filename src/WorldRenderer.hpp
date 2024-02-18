#pragma once
#include "Mat.hpp"
#include "WorldGeometryGenerator.hpp"
#include "WorldTexturesManager.hpp"

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
	const uint32_t queue_family_index_;

	WorldGeometryGenerator geometry_generator_;
	WorldTexturesManager world_textures_manager_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;

	vk::UniqueSampler texture_sampler_;

	vk::UniqueDescriptorSetLayout decriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;

	vk::UniqueDescriptorPool descriptor_pool_;
	vk::UniqueDescriptorSet descriptor_set_;

	vk::UniqueBuffer index_buffer_;
	vk::UniqueDeviceMemory index_buffer_memory_;
};

} // namespace HexGPU
