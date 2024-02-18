#pragma once
#include "Vec.hpp"
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldProcessor
{
public:
	WorldProcessor(WindowVulkan& window_vulkan);
	~WorldProcessor();

	void Update(
		vk::CommandBuffer command_buffer,
		const m_Vec3& player_pos,
		const m_Vec3& player_dir,
		bool build_triggered,
		bool destroy_triggered);

	vk::Buffer GetChunkDataBuffer() const;
	uint32_t GetChunkDataBufferSize() const;

	vk::Buffer GetPlayerStateBuffer() const;

public:
	// This struct must be identical to the same struct in GLSL code!
	struct PlayerState
	{
		int32_t build_pos[4];
		int32_t destroy_pos[4];
	};

private:
	const vk::Device vk_device_;
	const uint32_t vk_queue_family_index_;

	vk::UniqueBuffer vk_chunk_data_buffer_;
	vk::UniqueDeviceMemory vk_chunk_data_buffer_memory_;
	uint32_t chunk_data_buffer_size_= 0;

	vk::UniqueBuffer vk_player_state_buffer_;
	vk::UniqueDeviceMemory vk_player_state_buffer_memory_;

	vk::UniqueShaderModule world_gen_shader_;

	vk::UniqueDescriptorSetLayout vk_world_gen_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_world_gen_pipeline_layout_;
	vk::UniquePipeline vk_world_gen_pipeline_;
	vk::UniqueDescriptorPool vk_world_gen_descriptor_pool_;
	vk::UniqueDescriptorSet vk_world_gen_descriptor_set_;

	vk::UniqueShaderModule player_update_shader_;

	vk::UniqueDescriptorSetLayout vk_player_update_decriptor_set_layout_;
	vk::UniquePipelineLayout vk_player_update_pipeline_layout_;
	vk::UniquePipeline vk_player_update_pipeline_;
	vk::UniqueDescriptorPool vk_player_update_descriptor_pool_;
	vk::UniqueDescriptorSet vk_player_update_descriptor_set_;

	bool world_generated_= false;
};

} // namespace HexGPU
