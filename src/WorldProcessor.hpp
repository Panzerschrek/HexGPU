#pragma once
#include "BlockType.hpp"
#include "Vec.hpp"
#include "WindowVulkan.hpp"

namespace HexGPU
{

using WorldSizeChunks= std::array<uint32_t, 2>;
using WorldOffsetChunks= std::array<int32_t, 2>;

class WorldProcessor
{
public:
	WorldProcessor(WindowVulkan& window_vulkan, vk::DescriptorPool global_descriptor_pool, Settings& settings);
	~WorldProcessor();

	void Update(
		vk::CommandBuffer command_buffer,
		float time_delta_s,
		const m_Vec3& player_pos,
		const m_Vec2& player_angles,
		BlockType build_block_type,
		bool build_triggered,
		bool destroy_triggered,
		float aspect);

	vk::Buffer GetChunkDataBuffer(uint32_t index) const;
	uint32_t GetChunkDataBufferSize() const;
	vk::Buffer GetLightDataBuffer(uint32_t index) const;
	uint32_t GetLightDataBufferSize() const;

	vk::Buffer GetPlayerStateBuffer() const;

	WorldSizeChunks GetWorldSize() const;
	WorldOffsetChunks GetWorldOffset() const;

	uint32_t GetActualBuffersIndex() const;

public:
	// This struct must be identical to the same struct in GLSL code!
	struct PlayerState
	{
		float blocks_matrix[16]{};
		int32_t build_pos[4]; // component 3 - direction
		int32_t destroy_pos[4];
	};

private:
	struct BufferWithMemory
	{
		vk::UniqueBuffer buffer;
		vk::UniqueDeviceMemory memory;
	};

	// These constants must be the same in GLSL code!
	static constexpr uint32_t c_player_world_window_size[3]{16, 16, 16};
	static constexpr uint32_t c_player_world_window_volume= c_player_world_window_size[0] * c_player_world_window_size[1] * c_player_world_window_size[2];

	// This struct must be identical to the same struct in GLSL code!
	struct PlayerWorldWindow
	{
		int32_t offset[4]{}; // Position of the window start (in blocks)
		uint8_t window_data[c_player_world_window_volume];
	};

	// This struct must be identical to the same struct in GLSL code!
	struct WorldBlockExternalUpdate
	{
		uint32_t position[4]{};
		BlockType old_block_type= BlockType::Air;
		BlockType new_block_type= BlockType::Air;
		uint8_t reserved[2];
	};

	static constexpr uint32_t c_max_world_blocks_external_updates= 64;

	// This struct must be identical to the same struct in GLSL code!
	struct WorldBlocksExternalUpdateQueue
	{
		uint32_t num_updates= 0;
		WorldBlockExternalUpdate updates[c_max_world_blocks_external_updates];
	};

private:
	void InitialFillBuffers(vk::CommandBuffer command_buffer);
	void GenerateWorld(vk::CommandBuffer command_buffer);
	void BuildCurrentFrameChunksToUpdateList(float prev_offset_within_tick, float cur_offset_within_tick);
	void UpdateWorldBlocks(vk::CommandBuffer command_buffer);
	void UpdateLight(vk::CommandBuffer command_buffer);
	void CreateWorldBlocksAndLightUpdateBarrier(vk::CommandBuffer command_buffer);
	void BuildPlayerWorldWindow(vk::CommandBuffer command_buffer, const m_Vec3& player_pos);
	void UpdatePlayer(
		vk::CommandBuffer command_buffer,
		const m_Vec3& player_pos,
		const m_Vec2& player_angles,
		BlockType build_block_type,
		bool build_triggered,
		bool destroy_triggered,
		float aspect);
	void FlushWorldBlocksExternalUpdateQueue(vk::CommandBuffer command_buffer);

	uint32_t GetSrcBufferIndex() const;
	uint32_t GetDstBufferIndex() const;

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const WorldSizeChunks world_size_;

	WorldOffsetChunks world_offset_;

	// Use double buffering for world update.
	// On each step data is read from one of them and written into another.
	BufferWithMemory chunk_data_buffers_[2];
	uint32_t chunk_data_buffer_size_= 0;

	// Use double buffering for light update.
	// On each step data is read from one of them and written into another.
	BufferWithMemory light_buffers_[2];
	uint32_t light_buffer_size_= 0;

	BufferWithMemory player_state_buffer_;
	BufferWithMemory world_blocks_external_update_queue_buffer_;
	BufferWithMemory player_world_window_buffer_;

	vk::UniqueShaderModule world_gen_shader_;
	vk::UniqueDescriptorSetLayout world_gen_decriptor_set_layout_;
	vk::UniquePipelineLayout world_gen_pipeline_layout_;
	vk::UniquePipeline world_gen_pipeline_;
	vk::DescriptorSet world_gen_descriptor_sets_[2];

	vk::UniqueShaderModule initial_light_fill_shader_;
	vk::UniqueDescriptorSetLayout initial_light_fill_decriptor_set_layout_;
	vk::UniquePipelineLayout initial_light_fill_pipeline_layout_;
	vk::UniquePipeline initial_light_fill_pipeline_;
	vk::DescriptorSet initial_light_fill_descriptor_sets_[2];

	vk::UniqueShaderModule world_blocks_update_shader_;
	vk::UniqueDescriptorSetLayout world_blocks_update_decriptor_set_layout_;
	vk::UniquePipelineLayout world_blocks_update_pipeline_layout_;
	vk::UniquePipeline world_blocks_update_pipeline_;
	vk::DescriptorSet world_blocks_update_descriptor_sets_[2];

	vk::UniqueShaderModule light_update_shader_;
	vk::UniqueDescriptorSetLayout light_update_decriptor_set_layout_;
	vk::UniquePipelineLayout light_update_pipeline_layout_;
	vk::UniquePipeline light_update_pipeline_;
	vk::DescriptorSet light_update_descriptor_sets_[2];

	vk::UniqueShaderModule player_world_window_build_shader_;
	vk::UniqueDescriptorSetLayout player_world_window_build_decriptor_set_layout_;
	vk::UniquePipelineLayout player_world_window_build_pipeline_layout_;
	vk::UniquePipeline player_world_window_build_pipeline_;
	vk::DescriptorSet player_world_window_build_descriptor_sets_[2];

	vk::UniqueShaderModule player_update_shader_;
	vk::UniqueDescriptorSetLayout player_update_decriptor_set_layout_;
	vk::UniquePipelineLayout player_update_pipeline_layout_;
	vk::UniquePipeline player_update_pipeline_;
	vk::DescriptorSet player_update_descriptor_set_;

	vk::UniqueShaderModule world_blocks_external_update_queue_flush_shader_;
	vk::UniqueDescriptorSetLayout world_blocks_external_update_queue_flush_decriptor_set_layout_;
	vk::UniquePipelineLayout world_blocks_external_update_queue_flush_pipeline_layout_;
	vk::UniquePipeline world_blocks_external_update_queue_flush_pipeline_;
	vk::DescriptorSet world_blocks_external_update_queue_flush_descriptor_sets_[2];

	bool initial_buffers_filled_= false;
	bool world_generated_= false;

	uint32_t current_tick_= 0;
	float current_tick_fractional_= 0.0f;
	// Update this list each frame.
	std::vector<std::array<uint32_t, 2>> current_frame_chunks_to_update_list_;
};

} // namespace HexGPU
