#pragma once
#include "BlockType.hpp"
#include "ChunksStorage.hpp"
#include "DebugParams.hpp"
#include "GPUDataUploader.hpp"
#include "Keyboard.hpp"
#include "Mouse.hpp"
#include "Pipeline.hpp"
#include "StructuresBuffer.hpp"
#include "TaskOrganizer.hpp"
#include "TreesDistribution.hpp"

namespace HexGPU
{

using WorldSizeChunks= std::array<uint32_t, 2>;
using WorldOffsetChunks= std::array<int32_t, 2>;

class WorldProcessor
{
public:
	// This struct must be identical to the same struct in GLSL code!
	struct PlayerState
	{
		float blocks_matrix[16]{};
		float fog_matrix[16]{};
		float sky_matrix[16]{};
		float fog_color[4]{};
		float frustum_planes[5][4]{};
		float pos[4]{};
		float angles[4]{};
		float velocity[4]{};
		int32_t build_pos[4]{}; // component 3 - direction
		int32_t destroy_pos[4]{};
		int32_t next_player_world_window_offset[4]{};
		BlockType build_block_type= BlockType::Air;
	};

	// This struct must be identical to the same struct in GLSL code!
	struct WorldGlobalState
	{
		float sky_light_color[4]{};
		float sky_color[4]{};
		float sun_direction[4]{};
		float clouds_color[4]{};
	};

public:
	WorldProcessor(
		WindowVulkan& window_vulkan,
		GPUDataUploader& gpu_data_uploader,
		vk::DescriptorPool global_descriptor_pool,
		Settings& settings);
	~WorldProcessor();

	void Update(
		TaskOrganizer& task_organizer,
		float time_delta_s,
		KeyboardState keyboard_state,
		MouseState mouse_state,
		std::array<float, 2> mouse_move,
		BlockType selected_block_type, // Air if no selection.
		float aspect,
		const DebugParams& debug_params);

	vk::Buffer GetChunkDataBuffer(uint32_t index) const;
	vk::DeviceSize GetChunkDataBufferSize() const;

	vk::Buffer GetChunkAuxiliarDataBuffer(uint32_t index) const;
	vk::DeviceSize GetChunkAuxiliarDataBufferSize() const;

	vk::Buffer GetLightDataBuffer(uint32_t index) const;
	vk::DeviceSize GetLightDataBufferSize() const;

	vk::Buffer GetPlayerStateBuffer() const;

	vk::Buffer GetWorldGlobalStateBuffer() const;

	WorldSizeChunks GetWorldSize() const;
	WorldOffsetChunks GetWorldOffset() const;

	uint32_t GetActualBuffersIndex() const;

	// Returns player state or null.
	// Player state is read back from the GPU and is a couple of frames outdated.
	const PlayerState* GetLastKnownPlayerState() const;

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

	struct ChunkStructureDescription
	{
		int8_t min[4]{};
		int8_t max[4]{};
	};

	static constexpr uint32_t c_max_chunk_structures= 32;

	struct ChunkGenInfo
	{
		uint32_t num_structures= 0;
		uint32_t reserved[3]{};
		ChunkStructureDescription structures[c_max_chunk_structures];
	};

	using RelativeWorldShiftChunks= std::array<int32_t, 2>;

	enum class ChunkUpdateKind : uint8_t
	{
		Update,
		Generate,
		Upload,
	};

private:
	void InitialFillBuffers(TaskOrganizer& task_organizer);

	void ReadBackAndProcessPlayerState();

	void InitialFillWorld(TaskOrganizer& task_organizer);

	void DetermineChunksUpdateKind(RelativeWorldShiftChunks relative_world_shift);
	void BuildCurrentFrameChunksToUpdateList(float prev_offset_within_tick, float cur_offset_within_tick);

	void UpdateWorldGlobalState(TaskOrganizer& task_organizer, const DebugParams& debug_params);
	void UpdateWorldBlocks(TaskOrganizer& task_organizer, RelativeWorldShiftChunks relative_world_shift);
	void UpdateLight(TaskOrganizer& task_organizer, RelativeWorldShiftChunks relative_world_shift);
	void GenerateWorld(TaskOrganizer& task_organizer, RelativeWorldShiftChunks relative_world_shift);
	void DownloadChunks(TaskOrganizer& task_organizer);

	void FinishChunksDownloading(TaskOrganizer& task_organizer);
	void UploadChunks(TaskOrganizer& task_organizer);

	void BuildPlayerWorldWindow(TaskOrganizer& task_organizer);

	void UpdatePlayer(
		TaskOrganizer& task_organizer,
		float time_delta_s,
		KeyboardState keyboard_state,
		MouseState mouse_state,
		std::array<float, 2> mouse_move,
		BlockType selected_block_type,
		float aspect);

	void FlushWorldBlocksExternalUpdateQueue(TaskOrganizer& task_organizer);

	uint32_t GetSrcBufferIndex() const;
	uint32_t GetDstBufferIndex() const;

private:
	const vk::Device vk_device_;
	const vk::CommandPool command_pool_;
	const vk::Queue queue_;
	const uint32_t queue_family_index_;

	const WorldSizeChunks world_size_;
	const int32_t world_seed_;

	const StructuresBuffer structures_buffer_;

	const Buffer tree_map_buffer_;
	const Buffer chunk_gen_info_buffer_;

	// Use double buffering for world update.
	// On each step data is read from one of them and written into another.
	const std::array<Buffer, 2> chunk_data_buffers_;
	const std::array<Buffer, 2> chunk_auxiliar_data_buffers_; // Buffer for additional data for some types of blocks.

	// Buffer for chunk data downloading/uploading.
	const Buffer chunk_data_load_buffer_;
	void* const chunk_data_load_buffer_mapped_;
	const Buffer chunk_auxiliar_data_load_buffer_;
	void* const chunk_auxiliar_data_load_buffer_mapped_;

	// Use double buffering for light update.
	// On each step data is read from one of them and written into another.
	const std::array<Buffer, 2> light_buffers_;

	const Buffer world_global_state_buffer_;

	const Buffer player_state_buffer_;
	const Buffer world_blocks_external_update_queue_buffer_;
	const Buffer player_world_window_buffer_;

	const uint32_t player_state_read_back_buffer_num_frames_;
	const Buffer player_state_read_back_buffer_;
	const void* const player_state_read_back_buffer_mapped_;

	const ComputePipeline chunk_gen_prepare_pipeline_;
	const vk::DescriptorSet chunk_gen_prepare_descriptor_set_;

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

	const ComputePipeline world_global_state_update_pipeline_;
	const vk::DescriptorSet world_global_state_update_descriptor_set_;

	const vk::UniqueEvent chunk_data_download_event_;

	ChunkDataCompressor chunk_data_compressor_;
	ChunksStorage chunks_storage_;

	WorldOffsetChunks world_offset_; // Current offset
	WorldOffsetChunks next_world_offset_; // Offset which will be current at the start of the next tick
	WorldOffsetChunks next_next_world_offset_; // Offset whic will be next at the start of the next tick

	bool initial_buffers_filled_= false;

	// Update frame. Incremented on each "update" call.
	uint32_t current_frame_= 0;

	// Tick number. Incrementing at new tick start only (not each frame).
	uint32_t current_tick_= 0;
	float current_tick_fractional_= 0.0f;

	// Update this list each frame. Includes both chunks for update and generation.
	std::vector<std::array<uint32_t, 2>> current_frame_chunks_to_update_list_;

	// Update kind for each chunk in this tick.
	std::vector<ChunkUpdateKind> chunks_upate_kind_;

	std::optional<PlayerState> last_known_player_state_;

	bool wait_for_chunks_data_download_= false;
};

} // namespace HexGPU
