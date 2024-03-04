#pragma once
#include "BlockType.hpp"
#include "Buffer.hpp"
#include "Keyboard.hpp"
#include "Mouse.hpp"
#include "Pipeline.hpp"
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
		KeyboardState keyboard_state,
		MouseState mouse_state,
		float aspect);

	void StepWorldEast();
	void StepWorldWest();
	void StepWorldNorth();
	void StepWorldSouth();

	vk::Buffer GetChunkDataBuffer(uint32_t index) const;
	vk::DeviceSize GetChunkDataBufferSize() const;
	vk::Buffer GetLightDataBuffer(uint32_t index) const;
	vk::DeviceSize GetLightDataBufferSize() const;

	vk::Buffer GetPlayerStateBuffer() const;

	WorldSizeChunks GetWorldSize() const;
	WorldOffsetChunks GetWorldOffset() const;

	uint32_t GetActualBuffersIndex() const;

public:
	// This struct must be identical to the same struct in GLSL code!
	struct PlayerState
	{
		float blocks_matrix[16]{};
		float pos[4]{};
		float angles[4]{};
		int32_t build_pos[4]{}; // component 3 - direction
		int32_t destroy_pos[4]{};
		int32_t next_player_world_window_offset[4]{};
		BlockType build_block_type= BlockType::Air;
	};

private:
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

	using RelativeWorldShiftChunks= std::array<int32_t, 2>;

	enum class ChunkUpdateKind : uint8_t
	{
		Update,
		Generate,
	};

private:
	void InitialFillBuffers(vk::CommandBuffer command_buffer);
	void InitialGenerateWorld(vk::CommandBuffer command_buffer);
	void DetermineChunksUpdateKind(RelativeWorldShiftChunks relative_world_shift);
	void BuildCurrentFrameChunksToUpdateList(float prev_offset_within_tick, float cur_offset_within_tick);
	void UpdateWorldBlocks(vk::CommandBuffer command_buffer, RelativeWorldShiftChunks relative_world_shift);
	void UpdateLight(vk::CommandBuffer command_buffer, RelativeWorldShiftChunks relative_world_shift);
	void GenerateWorld(vk::CommandBuffer command_buffer, RelativeWorldShiftChunks relative_world_shift);
	void CreateWorldBlocksAndLightUpdateBarrier(vk::CommandBuffer command_buffer);
	void BuildPlayerWorldWindow(vk::CommandBuffer command_buffer);
	void UpdatePlayer(
		vk::CommandBuffer command_buffer,
		float time_delta_s,
		KeyboardState keyboard_state,
		MouseState mouse_state,
		float aspect);
	void FlushWorldBlocksExternalUpdateQueue(vk::CommandBuffer command_buffer);

	uint32_t GetSrcBufferIndex() const;
	uint32_t GetDstBufferIndex() const;

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const WorldSizeChunks world_size_;

	// Use double buffering for world update.
	// On each step data is read from one of them and written into another.
	const std::array<Buffer, 2> chunk_data_buffers_;

	// Use double buffering for light update.
	// On each step data is read from one of them and written into another.
	const std::array<Buffer, 2> light_buffers_;

	const Buffer player_state_buffer_;
	const Buffer world_blocks_external_update_queue_buffer_;
	const Buffer player_world_window_buffer_;

	const ComputePipeline world_gen_pipeline_;
	const std::array<vk::DescriptorSet, 2> world_gen_descriptor_sets_;

	const ComputePipeline initial_light_fill_pipeline_;
	const std::array<vk::DescriptorSet, 2> initial_light_fill_descriptor_sets_;

	const ComputePipeline world_blocks_update_pipeline_;
	const std::array<vk::DescriptorSet, 2> world_blocks_update_descriptor_sets_;

	const ComputePipeline light_update_pipeline_;
	const std::array<vk::DescriptorSet, 2> light_update_descriptor_sets_;

	const ComputePipeline player_world_window_build_pipeline_;
	const std::array<vk::DescriptorSet, 2> player_world_window_build_descriptor_sets_;

	const ComputePipeline player_update_pipeline_;
	const vk::DescriptorSet player_update_descriptor_set_;

	const ComputePipeline world_blocks_external_update_queue_flush_pipeline_;
	const std::array<vk::DescriptorSet, 2> world_blocks_external_update_queue_flush_descriptor_sets_;

	WorldOffsetChunks world_offset_; // Current offset
	WorldOffsetChunks next_world_offset_; // Offset which will be current at the start of the next tick
	WorldOffsetChunks next_next_world_offset_; // Offset whic will be next at the start of the next tick

	bool initial_buffers_filled_= false;

	uint32_t current_tick_= 0;
	float current_tick_fractional_= 0.0f;

	// Update this list each frame. Includes both chunks for update and generation.
	std::vector<std::array<uint32_t, 2>> current_frame_chunks_to_update_list_;

	// Update kind for each chunk in this tick.
	std::vector<ChunkUpdateKind> chunks_upate_kind_;
};

} // namespace HexGPU
