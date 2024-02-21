#pragma once
#include "WorldProcessor.hpp"

namespace HexGPU
{

struct WorldVertex
{
	// Use 16 bit for position in order to make this struct more compact (relative to floats).
	int16_t pos[4];
	int16_t tex_coord[4]; // component 3 - light
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
	const uint32_t queue_family_index_;
	WorldProcessor& world_processor_;

	uint32_t chunk_draw_info_buffer_size_= 0;
	vk::UniqueBuffer chunk_draw_info_buffer_;
	vk::UniqueDeviceMemory chunk_draw_info_buffer_memory_;
	bool chunk_draw_info_buffer_initially_filled_= false;


	size_t vertex_buffer_num_quads_= 0;
	vk::UniqueBuffer vertex_buffer_;
	vk::UniqueDeviceMemory vertex_buffer_memory_;
	bool vertex_buffer_initially_filled_= false;

	vk::UniqueBuffer draw_indirect_buffer_;
	vk::UniqueDeviceMemory draw_indirect_buffer_memory_;

	vk::UniqueShaderModule geometry_gen_shader_;
	vk::UniqueDescriptorSetLayout geometry_gen_decriptor_set_layout_;
	vk::UniquePipelineLayout geometry_gen_pipeline_layout_;
	vk::UniquePipeline geometry_gen_pipeline_;
	vk::UniqueDescriptorPool geometry_gen_descriptor_pool_;
	vk::UniqueDescriptorSet geometry_gen_descriptor_set_;

	vk::UniqueShaderModule draw_indirect_buffer_build_shader_;
	vk::UniqueDescriptorSetLayout draw_indirect_buffer_build_decriptor_set_layout_;
	vk::UniquePipelineLayout draw_indirect_buffer_build_pipeline_layout_;
	vk::UniquePipeline draw_indirect_buffer_build_pipeline_;
	vk::UniqueDescriptorPool draw_indirect_buffer_build_descriptor_pool_;
	vk::UniqueDescriptorSet draw_indirect_buffer_build_descriptor_set_;
};

} // namespace HexGPU
