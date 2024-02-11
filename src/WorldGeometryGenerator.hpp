#pragma once
#include "WindowVulkan.hpp"

namespace HexGPU
{

struct WorldVertex
{
	float pos[4];
	uint8_t color[4];
	uint8_t reserved[12];
};

using QuadVertices= std::array<WorldVertex, 4>;

class WorldGeometryGenerator
{
public:
	WorldGeometryGenerator(WindowVulkan& window_vulkan);
	~WorldGeometryGenerator();

	void PrepareFrame(vk::CommandBuffer command_buffer);

	vk::Buffer GetVertexBuffer() const;
	vk::Buffer GetDrawIndirectBuffer() const;

private:
	const vk::Device vk_device_;

	vk::UniqueShaderModule geometry_gen_shader_;

	vk::UniqueBuffer vk_vertex_buffer_;
	vk::UniqueDeviceMemory vk_vertex_buffer_memory_;

	vk::UniqueBuffer vk_draw_indirect_buffer_;
	vk::UniqueDeviceMemory vk_draw_indirect_buffer_memory_;

	vk::UniqueDescriptorSetLayout vk_geometry_gen_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_geometry_gen_pipeline_layout_;
	vk::UniquePipeline vk_geometry_gen_pipeline_;
	vk::UniqueDescriptorPool vk_geometry_gen_descriptor_pool_;
	vk::UniqueDescriptorSet vk_geometry_gen_descriptor_set_;
};

} // namespace HexGPU
