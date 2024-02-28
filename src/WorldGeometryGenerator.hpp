#pragma once
#include "GPUAllocator.hpp"
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
	// If this changed, the same struct in GLSL code must be changed too!
	struct ChunkDrawInfo
	{
		uint32_t num_quads= 0;
		uint32_t new_num_quads= 0;
		uint32_t first_quad= 0;
		uint32_t first_memory_unit= 0;
		uint32_t num_memory_units= 0;
	};

public:
	WorldGeometryGenerator(
		WindowVulkan& window_vulkan,
		WorldProcessor& world_processor,
		vk::DescriptorPool global_descriptor_pool);
	~WorldGeometryGenerator();

	void Update(vk::CommandBuffer command_buffer);

	vk::Buffer GetVertexBuffer() const;

	vk::Buffer GetChunkDrawInfoBuffer() const;
	uint32_t GetChunkDrawInfoBufferSize() const;

private:
	void InitialFillBuffers(vk::CommandBuffer command_buffer);
	void BuildChunksToUpdateList();
	void PrepareGeometrySizeCalculation(vk::CommandBuffer command_buffer);
	void CalculateGeometrySize(vk::CommandBuffer command_buffer);
	void AllocateMemoryForGeometry(vk::CommandBuffer command_buffer);
	void GenGeometry(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	WorldProcessor& world_processor_;
	const WorldSizeChunks world_size_;

	bool buffers_initially_filled_= false;

	uint32_t chunk_draw_info_buffer_size_= 0;
	vk::UniqueBuffer chunk_draw_info_buffer_;
	vk::UniqueDeviceMemory chunk_draw_info_buffer_memory_;

	size_t vertex_buffer_num_quads_= 0;
	vk::UniqueBuffer vertex_buffer_;
	vk::UniqueDeviceMemory vertex_buffer_memory_;

	GPUAllocator vertex_memory_allocator_;

	vk::UniqueShaderModule geometry_size_calculate_prepare_shader_;
	vk::UniqueDescriptorSetLayout geometry_size_calculate_prepare_decriptor_set_layout_;
	vk::UniquePipelineLayout geometry_size_calculate_prepare_pipeline_layout_;
	vk::UniquePipeline geometry_size_calculate_prepare_pipeline_;
	vk::DescriptorSet geometry_size_calculate_prepare_descriptor_set_;

	vk::UniqueShaderModule geometry_size_calculate_shader_;
	vk::UniqueDescriptorSetLayout geometry_size_calculate_decriptor_set_layout_;
	vk::UniquePipelineLayout geometry_size_calculate_pipeline_layout_;
	vk::UniquePipeline geometry_size_calculate_pipeline_;
	vk::DescriptorSet geometry_size_calculate_descriptor_set_;

	vk::UniqueShaderModule geometry_allocate_shader_;
	vk::UniqueDescriptorSetLayout geometry_allocate_decriptor_set_layout_;
	vk::UniquePipelineLayout geometry_allocate_pipeline_layout_;
	vk::UniquePipeline geometry_allocate_pipeline_;
	vk::DescriptorSet geometry_allocate_descriptor_set_;

	vk::UniqueShaderModule geometry_gen_shader_;
	vk::UniqueDescriptorSetLayout geometry_gen_decriptor_set_layout_;
	vk::UniquePipelineLayout geometry_gen_pipeline_layout_;
	vk::UniquePipeline geometry_gen_pipeline_;
	vk::DescriptorSet geometry_gen_descriptor_set_;

	uint32_t frame_counter_= 0;
	std::vector<std::array<uint32_t, 2>> chunks_to_update_;
};

} // namespace HexGPU
