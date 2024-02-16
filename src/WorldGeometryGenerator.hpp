#pragma once
#include "WorldProcessor.hpp"

namespace HexGPU
{

struct WorldVertex
{
	// Use 16 bit for position in order to make this struct more compact (relative to floats).
	int16_t pos[4];
	int16_t tex_coord[4];
};

using QuadVertices= std::array<WorldVertex, 4>;

class WorldGeometryGenerator
{
public:
	WorldGeometryGenerator(WindowVulkan& window_vulkan, WorldProcessor& world_processor);
	~WorldGeometryGenerator();

	void PrepareFrame(vk::CommandBuffer command_buffer);

	vk::Buffer GetVertexBuffer() const;
	vk::Buffer GetDrawIndirectBuffer() const;

private:
	const vk::Device vk_device_;
	const uint32_t vk_queue_family_index_;
	WorldProcessor& world_processor_;

	vk::UniqueShaderModule geometry_gen_shader_;

	size_t vertex_buffer_num_quads_= 0;
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
