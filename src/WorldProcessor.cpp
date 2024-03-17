#include "WorldProcessor.hpp"
#include "Assert.hpp"
#include "Constants.hpp"
#include "GlobalDescriptorPool.hpp"
#include "Log.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

namespace ChunkGenPrepareShaderBindings
{
	const ShaderBindingIndex chunk_gen_info_buffer= 0;
	const ShaderBindingIndex structure_descriptions_buffer= 1;
	const ShaderBindingIndex tree_map_buffer= 2;
}

namespace WorldGenShaderBindings
{
	const ShaderBindingIndex chunk_data_buffer= 0;
	const ShaderBindingIndex chunk_gen_info_buffer= 1;
	const ShaderBindingIndex structure_descriptions_buffer= 2;
	const ShaderBindingIndex structures_data_buffer= 3;
	const ShaderBindingIndex chunk_auxiliar_data_buffer= 4;
}

namespace InitialLightFillShaderBindings
{
	const ShaderBindingIndex chunk_data_buffer= 0;
	const ShaderBindingIndex light_data_buffer= 1;
}

namespace WorldBlocksUpdateShaderBindings
{
	const ShaderBindingIndex chunk_data_input_buffer= 0;
	const ShaderBindingIndex chunk_data_output_buffer= 1;
	const ShaderBindingIndex chunk_auxiliar_data_input_buffer= 2;
	const ShaderBindingIndex chunk_auxiliar_data_output_buffer= 3;
}

namespace LightUpdateShaderBindings
{
	const ShaderBindingIndex chunk_data_buffer= 0;
	const ShaderBindingIndex chunk_input_light_buffer= 1;
	const ShaderBindingIndex chunk_output_light_buffer= 2;
}

namespace PlayerWorldWindowBuildShaderBindings
{
	const ShaderBindingIndex chunk_data_buffer= 0;
	const ShaderBindingIndex player_world_window_buffer= 1;
	const ShaderBindingIndex player_state_buffer= 2;
}

namespace PlayerUpdateShaderBindings
{
	const ShaderBindingIndex player_state_buffer= 1;
	const ShaderBindingIndex world_blocks_external_update_queue_buffer= 2;
	const ShaderBindingIndex player_world_window_buffer= 3;
}

namespace WorldBlocksExternalUpdateQueueFlushShaderBindigns
{
	const ShaderBindingIndex chunk_data_buffer= 0;
	const ShaderBindingIndex world_blocks_external_update_queue_buffer= 1;
	const ShaderBindingIndex chunk_auxiliar_data_buffer= 2;
}

namespace WorldGlobalStateUpdateBindings
{
	const ShaderBindingIndex world_global_state_buffer= 0;
}

struct ChunkGenPrepareUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t chunk_position[2]{};
	int32_t chunk_global_position[2]{};
	int32_t seed= 0;
};

// This constant should match workgroup size in shader!
constexpr uint32_t c_world_gen_workgroup_size[]{8, 16, 1};
static_assert(c_chunk_width % c_world_gen_workgroup_size[0] == 0, "Wrong workgroup size!");
static_assert(c_chunk_width % c_world_gen_workgroup_size[1] == 0, "Wrong workgroup size!");
static_assert(c_world_gen_workgroup_size[2] == 1, "Wrong workgroup size!");

struct WorldGenUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t chunk_position[2]{};
	int32_t chunk_global_position[2]{};
	int32_t seed= 0;
};

// This constant should match workgroup size in shader!
constexpr uint32_t c_initial_light_fill_workgroup_size[]{8, 16, 1};
static_assert(c_chunk_width % c_initial_light_fill_workgroup_size[0] == 0, "Wrong workgroup size!");
static_assert(c_chunk_width % c_initial_light_fill_workgroup_size[1] == 0, "Wrong workgroup size!");
static_assert(c_initial_light_fill_workgroup_size[2] == 1, "Wrong workgroup size!");

struct InitialLightFillUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t chunk_position[2]{};
};

struct WorldBlocksUpdateUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t in_chunk_position[2]{};
	int32_t out_chunk_position[2]{};
	uint32_t current_tick= 0;
	uint32_t reserved= 0;
};

struct LightUpdateUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t in_chunk_position[2]{};
	int32_t out_chunk_position[2]{};
};

struct PlayerWorldWindowBuildUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t world_offset_chunks[2]{};
};

struct PlayerUpdateUniforms
{
	float aspect= 0.0f;
	float time_delta_s= 0.0f;
	KeyboardState keyboard_state= 0;
	MouseState mouse_state= 0;
};

struct WorldBlocksExternalUpdateQueueFlushUniforms
{
	int32_t world_size_chunks[2]{0, 0};
	int32_t world_offset_chunks[2]{0, 0};
};

struct WorldGlobalStateUpdateUniforms
{
	float time_of_day= 0.0f;
};

WorldSizeChunks ReadWorldSize(Settings& settings)
{
	// Round world size up to next even number.
	const int32_t world_size_x= (std::max(6, std::min(int32_t(settings.GetInt("g_world_size_x", 8)), 32)) + 1) & ~1;
	const int32_t world_size_y= (std::max(6, std::min(int32_t(settings.GetInt("g_world_size_y", 8)), 32)) + 1) & ~1;

	settings.SetInt("g_world_size_x", world_size_x);
	settings.SetInt("g_world_size_y", world_size_y);

	Log::Info("World size is ", world_size_x, "x", world_size_y, " chunks");

	return WorldSizeChunks{uint32_t(world_size_x), uint32_t(world_size_y)};
}

ComputePipeline CreateChunkGenPreparePipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::chunk_gen_prepare_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			ChunkGenPrepareShaderBindings::chunk_gen_info_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			ChunkGenPrepareShaderBindings::structure_descriptions_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			ChunkGenPrepareShaderBindings::tree_map_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(ChunkGenPrepareUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreateWorldGenPipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::world_gen_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			WorldGenShaderBindings::chunk_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			WorldGenShaderBindings::chunk_gen_info_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			WorldGenShaderBindings::structure_descriptions_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			WorldGenShaderBindings::structures_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			WorldGenShaderBindings::chunk_auxiliar_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(WorldGenUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreateInitialLightFillPipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::initial_light_fill_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			InitialLightFillShaderBindings::chunk_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			InitialLightFillShaderBindings::light_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(InitialLightFillUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreateWorldBlocksUpdatePipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::world_blocks_update_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			WorldBlocksUpdateShaderBindings::chunk_data_input_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			WorldBlocksUpdateShaderBindings::chunk_data_output_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			WorldBlocksUpdateShaderBindings::chunk_auxiliar_data_input_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			WorldBlocksUpdateShaderBindings::chunk_auxiliar_data_output_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(WorldBlocksUpdateUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreateLightUpdatePipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::light_update_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			LightUpdateShaderBindings::chunk_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			LightUpdateShaderBindings::chunk_input_light_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			LightUpdateShaderBindings::chunk_output_light_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(LightUpdateUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreatePlayerWorldWindowBuildPipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::player_world_window_build_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			PlayerWorldWindowBuildShaderBindings::chunk_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			PlayerWorldWindowBuildShaderBindings::player_world_window_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			PlayerWorldWindowBuildShaderBindings::player_state_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(PlayerWorldWindowBuildUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreatePlayerUpdatePipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::player_update_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			PlayerUpdateShaderBindings::player_state_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			PlayerUpdateShaderBindings::world_blocks_external_update_queue_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			PlayerUpdateShaderBindings::player_world_window_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(PlayerUpdateUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreateWorldBlocksExternalUpdateQueueFlushPipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::world_blocks_external_queue_flush_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			WorldBlocksExternalUpdateQueueFlushShaderBindigns::chunk_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			WorldBlocksExternalUpdateQueueFlushShaderBindigns::world_blocks_external_update_queue_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			WorldBlocksExternalUpdateQueueFlushShaderBindigns::chunk_auxiliar_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(WorldBlocksExternalUpdateQueueFlushUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreateWorldGlobalStateUpdatePipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::world_global_state_update_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			WorldGlobalStateUpdateBindings::world_global_state_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(WorldGlobalStateUpdateUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

Buffer CreateChunkDataBuffer(WindowVulkan& window_vulkan, const WorldSizeChunks& world_size)
{
	return Buffer(
		window_vulkan,
		c_chunk_volume * world_size[0] * world_size[1],
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc);
}

Buffer CreateLightBuffer(WindowVulkan& window_vulkan, const WorldSizeChunks& world_size)
{
	return Buffer(
		window_vulkan,
		c_chunk_volume * world_size[0] * world_size[1],
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst);
}

Buffer CreateChunkDataLoadBuffer(WindowVulkan& window_vulkan, const WorldSizeChunks& world_size)
{
	return Buffer(
		window_vulkan,
		c_chunk_volume * world_size[0] * world_size[1],
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
		// Use host-cached memory, because its usage by the host is much faster then host-coherent memory.
		// But the cost of this speed is the necessity to call InvalidateRanges/FlushRanges.
		// TODO - add a workaround for systems without host-cached memory.
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCached);
}

} // namespace

WorldProcessor::WorldProcessor(
	WindowVulkan& window_vulkan,
	const vk::DescriptorPool global_descriptor_pool,
	Settings& settings)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, command_pool_(window_vulkan.GetCommandPool())
	, queue_(window_vulkan.GetQueue())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_size_(ReadWorldSize(settings))
	, world_seed_(int32_t(settings.GetOrSetInt("g_world_seed")))
	, structures_buffer_(window_vulkan, GenStructures())
	, tree_map_buffer_(
		window_vulkan,
		sizeof(TreeMap),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, chunk_gen_info_buffer_(
		window_vulkan,
		sizeof(ChunkGenInfo) * world_size_[0] * world_size_[1],
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, chunk_data_buffers_{
		CreateChunkDataBuffer(window_vulkan, world_size_),
		CreateChunkDataBuffer(window_vulkan, world_size_)}
	, chunk_auxiliar_data_buffers_{
		CreateChunkDataBuffer(window_vulkan, world_size_),
		CreateChunkDataBuffer(window_vulkan, world_size_)}
	, chunk_data_load_buffer_(CreateChunkDataLoadBuffer(window_vulkan, world_size_))
	, chunk_data_load_buffer_mapped_(chunk_data_load_buffer_.Map(vk_device_))
	, chunk_auxiliar_data_load_buffer_(CreateChunkDataLoadBuffer(window_vulkan, world_size_))
	, chunk_auxiliar_data_load_buffer_mapped_(chunk_auxiliar_data_load_buffer_.Map(vk_device_))
	, light_buffers_{
		CreateLightBuffer(window_vulkan, world_size_),
		CreateLightBuffer(window_vulkan, world_size_)}
	, world_global_state_buffer_(
		window_vulkan,
		sizeof(WorldGlobalState),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst)
	, player_state_buffer_(
		window_vulkan,
		sizeof(PlayerState),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst)
	, world_blocks_external_update_queue_buffer_(
		window_vulkan,
		sizeof(WorldBlocksExternalUpdateQueue),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, player_world_window_buffer_(
		window_vulkan,
		sizeof(PlayerWorldWindow),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, player_state_read_back_buffer_num_frames_(uint32_t(window_vulkan.GetNumCommandBuffers()))
	, player_state_read_back_buffer_(
		window_vulkan,
		sizeof(PlayerState) * player_state_read_back_buffer_num_frames_,
		vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
	, player_state_read_back_buffer_mapped_(player_state_read_back_buffer_.Map(window_vulkan.GetVulkanDevice()))
	, chunk_gen_prepare_pipeline_(CreateChunkGenPreparePipeline(vk_device_))
	, chunk_gen_prepare_descriptor_set_(
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *chunk_gen_prepare_pipeline_.descriptor_set_layout))
	, world_gen_pipeline_(CreateWorldGenPipeline(vk_device_))
	, world_gen_descriptor_sets_{
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *world_gen_pipeline_.descriptor_set_layout),
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *world_gen_pipeline_.descriptor_set_layout)}
	, initial_light_fill_pipeline_(CreateInitialLightFillPipeline(vk_device_))
	, initial_light_fill_descriptor_sets_{
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *initial_light_fill_pipeline_.descriptor_set_layout),
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *initial_light_fill_pipeline_.descriptor_set_layout)}
	, world_blocks_update_pipeline_(CreateWorldBlocksUpdatePipeline(vk_device_))
	, world_blocks_update_descriptor_sets_{
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *world_blocks_update_pipeline_.descriptor_set_layout),
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *world_blocks_update_pipeline_.descriptor_set_layout)}
	, light_update_pipeline_(CreateLightUpdatePipeline(vk_device_))
	, light_update_descriptor_sets_{
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *light_update_pipeline_.descriptor_set_layout),
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *light_update_pipeline_.descriptor_set_layout)}
	, player_world_window_build_pipeline_(CreatePlayerWorldWindowBuildPipeline(vk_device_))
	, player_world_window_build_descriptor_sets_{
		CreateDescriptorSet(
			vk_device_,
			global_descriptor_pool,
			*player_world_window_build_pipeline_.descriptor_set_layout),
		CreateDescriptorSet(
			vk_device_,
			global_descriptor_pool,
			*player_world_window_build_pipeline_.descriptor_set_layout)}
	, player_update_pipeline_(CreatePlayerUpdatePipeline(vk_device_))
	, player_update_descriptor_set_(
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *player_update_pipeline_.descriptor_set_layout))
	, world_blocks_external_update_queue_flush_pipeline_(CreateWorldBlocksExternalUpdateQueueFlushPipeline(vk_device_))
	, world_blocks_external_update_queue_flush_descriptor_sets_{
		CreateDescriptorSet(
			vk_device_,
			global_descriptor_pool,
			*world_blocks_external_update_queue_flush_pipeline_.descriptor_set_layout),
		CreateDescriptorSet(
			vk_device_,
			global_descriptor_pool,
			*world_blocks_external_update_queue_flush_pipeline_.descriptor_set_layout)}
	, world_global_state_update_pipeline_(CreateWorldGlobalStateUpdatePipeline(vk_device_))
	, world_global_state_update_descriptor_set_(
		CreateDescriptorSet(
			vk_device_,
			global_descriptor_pool,
			*world_global_state_update_pipeline_.descriptor_set_layout))
	, chunk_data_download_event_(vk_device_.createEventUnique(vk::EventCreateInfo()))
	, chunks_storage_(settings)
	, world_offset_{-int32_t(world_size_[0] / 2u), -int32_t(world_size_[1] / 2u)}
	, next_world_offset_(world_offset_)
	, next_next_world_offset_(next_world_offset_)
{
	HEX_ASSERT(player_state_read_back_buffer_num_frames_ > 0);

	Log::Info("World seed: ", world_seed_);

	chunks_storage_.SetActiveArea(world_offset_, world_size_);

	// Update chunk gen prepare descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_gen_info_buffer(
			chunk_gen_info_buffer_.GetBuffer(),
			0u,
			chunk_gen_info_buffer_.GetSize());

		const vk::DescriptorBufferInfo descriptor_structure_descriptions_buffer(
			structures_buffer_.GetDescriptionsBuffer(),
			0u,
			structures_buffer_.GetDescriptionsBufferSize());

		const vk::DescriptorBufferInfo descriptor_tree_map_buffer(
			tree_map_buffer_.GetBuffer(),
			0u,
			tree_map_buffer_.GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					chunk_gen_prepare_descriptor_set_,
					ChunkGenPrepareShaderBindings::chunk_gen_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_gen_info_buffer,
					nullptr
				},
				{
					chunk_gen_prepare_descriptor_set_,
					ChunkGenPrepareShaderBindings::structure_descriptions_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_structure_descriptions_buffer,
					nullptr
				},
				{
					chunk_gen_prepare_descriptor_set_,
					ChunkGenPrepareShaderBindings::tree_map_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_tree_map_buffer,
					nullptr
				},
			},
			{});
	}

	// Update world generation descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].GetBuffer(),
			0u,
			chunk_data_buffers_[i].GetSize());

		const vk::DescriptorBufferInfo descriptor_chunk_gen_info_buffer(
			chunk_gen_info_buffer_.GetBuffer(),
			0u,
			chunk_gen_info_buffer_.GetSize());

		const vk::DescriptorBufferInfo descriptor_structure_descriptions_buffer(
			structures_buffer_.GetDescriptionsBuffer(),
			0u,
			structures_buffer_.GetDescriptionsBufferSize());

		const vk::DescriptorBufferInfo descriptor_structures_data_buffer(
			structures_buffer_.GetDataBuffer(),
			0u,
			structures_buffer_.GetDataBufferSize());

		const vk::DescriptorBufferInfo descriptor_chunk_auxiliar_data_buffer_info(
			chunk_auxiliar_data_buffers_[i].GetBuffer(),
			0u,
			chunk_auxiliar_data_buffers_[i].GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					world_gen_descriptor_sets_[i],
					WorldGenShaderBindings::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					world_gen_descriptor_sets_[i],
					WorldGenShaderBindings::chunk_gen_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_gen_info_buffer,
					nullptr
				},
				{
					world_gen_descriptor_sets_[i],
					WorldGenShaderBindings::structure_descriptions_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_structure_descriptions_buffer,
					nullptr
				},
				{
					world_gen_descriptor_sets_[i],
					WorldGenShaderBindings::structures_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_structures_data_buffer,
					nullptr
				},
				{
					world_gen_descriptor_sets_[i],
					WorldGenShaderBindings::chunk_auxiliar_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_auxiliar_data_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update initial light fill descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].GetBuffer(),
			0u,
			chunk_data_buffers_[i].GetSize());

		const vk::DescriptorBufferInfo descriptor_light_data_buffer_info(
			light_buffers_[i].GetBuffer(),
			0u,
			light_buffers_[i].GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					initial_light_fill_descriptor_sets_[i],
					InitialLightFillShaderBindings::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					initial_light_fill_descriptor_sets_[i],
					InitialLightFillShaderBindings::light_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_light_data_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update world blocks update descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_input_buffer_info(
			chunk_data_buffers_[i].GetBuffer(),
			0u,
			chunk_data_buffers_[i].GetSize());

		const vk::DescriptorBufferInfo descriptor_chunk_data_output_buffer_info(
			chunk_data_buffers_[i ^ 1].GetBuffer(),
			0u,
			chunk_data_buffers_[i ^ 1].GetSize());

		const vk::DescriptorBufferInfo descriptor_chunk_auxiliar_data_input_buffer_info(
			chunk_auxiliar_data_buffers_[i].GetBuffer(),
			0u,
			chunk_auxiliar_data_buffers_[i].GetSize());

		const vk::DescriptorBufferInfo descriptor_chunk_auxiliar_data_output_buffer_info(
			chunk_auxiliar_data_buffers_[i ^ 1].GetBuffer(),
			0u,
			chunk_auxiliar_data_buffers_[i ^ 1].GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					world_blocks_update_descriptor_sets_[i],
					WorldBlocksUpdateShaderBindings::chunk_data_input_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_input_buffer_info,
					nullptr
				},
				{
					world_blocks_update_descriptor_sets_[i],
					WorldBlocksUpdateShaderBindings::chunk_data_output_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_output_buffer_info,
					nullptr
				},
				{
					world_blocks_update_descriptor_sets_[i],
					WorldBlocksUpdateShaderBindings::chunk_auxiliar_data_input_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_auxiliar_data_input_buffer_info,
					nullptr
				},
				{
					world_blocks_update_descriptor_sets_[i],
					WorldBlocksUpdateShaderBindings::chunk_auxiliar_data_output_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_auxiliar_data_output_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update light update descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].GetBuffer(),
			0u,
			chunk_data_buffers_[i].GetSize());

		const vk::DescriptorBufferInfo descriptor_input_light_data_buffer_info(
			light_buffers_[i].GetBuffer(),
			0u,
			light_buffers_[i].GetSize());

		const vk::DescriptorBufferInfo descriptor_output_light_data_buffer_info(
			light_buffers_[i ^ 1].GetBuffer(),
			0u,
			light_buffers_[i ^ 1].GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					light_update_descriptor_sets_[i],
					LightUpdateShaderBindings::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					light_update_descriptor_sets_[i],
					LightUpdateShaderBindings::chunk_input_light_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_input_light_data_buffer_info,
					nullptr
				},
				{
					light_update_descriptor_sets_[i],
					LightUpdateShaderBindings::chunk_output_light_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_output_light_data_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update player world window build descriptor set.
	for(uint32_t i= 0; i < 2; ++i)
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].GetBuffer(),
			0u,
			chunk_data_buffers_[i].GetSize());

		const vk::DescriptorBufferInfo player_world_window_buffer_info(
			player_world_window_buffer_.GetBuffer(),
			0u,
			sizeof(PlayerWorldWindow));

		const vk::DescriptorBufferInfo player_state_buffer_info(
			player_state_buffer_.GetBuffer(),
			0u,
			sizeof(PlayerState));

		vk_device_.updateDescriptorSets(
			{
				{
					player_world_window_build_descriptor_sets_[i],
					PlayerWorldWindowBuildShaderBindings::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					player_world_window_build_descriptor_sets_[i],
					PlayerWorldWindowBuildShaderBindings::player_world_window_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&player_world_window_buffer_info,
					nullptr
				},
				{
					player_world_window_build_descriptor_sets_[i],
					PlayerWorldWindowBuildShaderBindings::player_state_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&player_state_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update player update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_player_state_buffer_info(
			player_state_buffer_.GetBuffer(),
			0u,
			sizeof(PlayerState));

		const vk::DescriptorBufferInfo world_blocks_external_update_queue_buffer_info(
			world_blocks_external_update_queue_buffer_.GetBuffer(),
			0u,
			sizeof(WorldBlocksExternalUpdateQueue));

		const vk::DescriptorBufferInfo player_world_window_buffer_info(
			player_world_window_buffer_.GetBuffer(),
			0u,
			sizeof(PlayerWorldWindow));

		vk_device_.updateDescriptorSets(
			{
				{
					player_update_descriptor_set_,
					PlayerUpdateShaderBindings::player_state_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_player_state_buffer_info,
					nullptr
				},
				{
					player_update_descriptor_set_,
					PlayerUpdateShaderBindings::world_blocks_external_update_queue_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&world_blocks_external_update_queue_buffer_info,
					nullptr
				},
				{
					player_update_descriptor_set_,
					PlayerUpdateShaderBindings::player_world_window_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&player_world_window_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update world blocks external update queue flush descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].GetBuffer(),
			0u,
			chunk_data_buffers_[i].GetSize());

		const vk::DescriptorBufferInfo world_blocks_external_update_queue_buffer_info(
			world_blocks_external_update_queue_buffer_.GetBuffer(),
			0u,
			sizeof(WorldBlocksExternalUpdateQueue));

		const vk::DescriptorBufferInfo descriptor_chunk_auxiliar_data_buffer_info(
			chunk_auxiliar_data_buffers_[i].GetBuffer(),
			0u,
			chunk_auxiliar_data_buffers_[i].GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					world_blocks_external_update_queue_flush_descriptor_sets_[i],
					WorldBlocksExternalUpdateQueueFlushShaderBindigns::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					world_blocks_external_update_queue_flush_descriptor_sets_[i],
					WorldBlocksExternalUpdateQueueFlushShaderBindigns::world_blocks_external_update_queue_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&world_blocks_external_update_queue_buffer_info,
					nullptr
				},
				{
					world_blocks_external_update_queue_flush_descriptor_sets_[i],
					WorldBlocksExternalUpdateQueueFlushShaderBindigns::chunk_auxiliar_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_auxiliar_data_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update world global state update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_world_global_state_buffer_info(
			world_global_state_buffer_.GetBuffer(),
			0u,
			world_global_state_buffer_.GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					world_global_state_update_descriptor_set_,
					WorldGlobalStateUpdateBindings::world_global_state_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_world_global_state_buffer_info,
					nullptr
				},
			},
			{});
	}
}

WorldProcessor::~WorldProcessor()
{
	// Wait for all commands to be finished.
	vk_device_.waitIdle();

	// Download all chunk data in order to save it.

	// Create one-time command buffer. TODO - avoid this?
	const auto command_buffer= std::move(vk_device_.allocateCommandBuffersUnique(
		vk::CommandBufferAllocateInfo(
			command_pool_,
			vk::CommandBufferLevel::ePrimary,
			1u)).front());

	command_buffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	const uint32_t src_buffer_index= GetSrcBufferIndex();

	// Copy all all contents of chunk data buffers.
	command_buffer->copyBuffer(
		chunk_data_buffers_[src_buffer_index].GetBuffer(),
		chunk_data_load_buffer_.GetBuffer(),
		{{0, 0, chunk_data_load_buffer_.GetSize()}});

	command_buffer->copyBuffer(
		chunk_auxiliar_data_buffers_[src_buffer_index].GetBuffer(),
		chunk_auxiliar_data_load_buffer_.GetBuffer(),
		{{0, 0, chunk_auxiliar_data_load_buffer_.GetSize()}});

	command_buffer->end();

	const vk::PipelineStageFlags wait_dst_stage_mask= vk::PipelineStageFlagBits::eTransfer;
	const vk::SubmitInfo submit_info(
		0u, nullptr,
		&wait_dst_stage_mask,
		1u, &*command_buffer,
		0u, nullptr);

	queue_.submit(submit_info, vk::Fence());

	// Wait until all is finished.
	queue_.waitIdle();

	// Invalidate ranges before reading.
	const vk::MappedMemoryRange mapped_memory_ranges[]
	{
		{chunk_data_load_buffer_.GetMemory(), 0, chunk_data_load_buffer_.GetSize()},
		{chunk_auxiliar_data_load_buffer_.GetMemory(), 0, chunk_auxiliar_data_load_buffer_.GetSize()},
	};
	vk_device_.invalidateMappedMemoryRanges(uint32_t(std::size(mapped_memory_ranges)), mapped_memory_ranges);

	// Compress and store chunks data.
	for(uint32_t y= 0; y < world_size_[1]; ++y)
	for(uint32_t x= 0; x < world_size_[0]; ++x)
	{
		const uint32_t chunk_index= x + y * world_size_[0];
		const uint32_t offset= chunk_index * c_chunk_volume;

		chunks_storage_.SetChunk(
			{
				int32_t(x) + world_offset_[0],
				int32_t(y) + world_offset_[1]
			},
			chunk_data_compressor_.Compress(
				static_cast<const BlockType*>(chunk_data_load_buffer_mapped_) + offset,
				static_cast<const uint8_t*>(chunk_auxiliar_data_load_buffer_mapped_) + offset));
	}

	// Unmap remaining buffers.
	chunk_data_load_buffer_.Unmap(vk_device_);
	chunk_auxiliar_data_load_buffer_.Unmap(vk_device_);

	player_state_read_back_buffer_.Unmap(vk_device_);
}

void WorldProcessor::Update(
	TaskOrganizer& task_organizer,
	const float time_delta_s,
	const KeyboardState keyboard_state,
	const MouseState mouse_state,
	const float aspect,
	const DebugParams& debug_params)
{
	InitialFillBuffers(task_organizer);

	ReadBackAndProcessPlayerState();

	const RelativeWorldShiftChunks relative_shift
	{
		next_world_offset_[0] - world_offset_[0],
		next_world_offset_[1] - world_offset_[1],
	};

	DetermineChunksUpdateKind(relative_shift);

	FinishChunksDownloading(task_organizer);

	// This frequency allows relatively fast world updates and still doesn't overload GPU too much.
	const float c_update_frequency= 8.0f;

	// Do not allow updating slightly less than whole world in a frame.
	const float tick_delta_clamped= std::min(time_delta_s * c_update_frequency, 0.75f);

	const float prev_tick_fractional= current_tick_fractional_;
	const float cur_tick_fractional= current_tick_fractional_ + tick_delta_clamped;

	if(current_tick_ == 0)
		InitialFillWorld(task_organizer);
	else
	{
		// Continue updates of blocks and lighting.
		const float prev_offset_within_tick= std::min(1.0f, prev_tick_fractional - float(current_tick_));
		const float cur_offset_within_tick= std::min(1.0f, cur_tick_fractional - float(current_tick_));
		BuildCurrentFrameChunksToUpdateList(prev_offset_within_tick, cur_offset_within_tick);

		UpdateWorldBlocks(task_organizer, relative_shift);
		UpdateLight(task_organizer, relative_shift);
		GenerateWorld(task_organizer, relative_shift);
		// No need to synchronize world blocks and lighting update here.
		// Add a barier only at the beginning of next tick.
	}

	// Prevent finishing tick if chunks data downloading/uploading operations are not finished yer.
	const bool is_time_to_switch_to_next_tick=
		uint32_t(prev_tick_fractional) < uint32_t(cur_tick_fractional) &&
		!wait_for_chunks_data_download_;

	// Switch to the next tick (if necessary).
	if(current_tick_ == 0 || is_time_to_switch_to_next_tick)
	{
		world_offset_= next_world_offset_;
		next_world_offset_= next_next_world_offset_;

		FlushWorldBlocksExternalUpdateQueue(task_organizer);

		++current_tick_;
		current_tick_fractional_= float(current_tick_);

		BuildPlayerWorldWindow(task_organizer);

		// Update world global state once in a tick.
		UpdateWorldGlobalState(task_organizer, debug_params);

		// Download (if necessary) chunks which are no longer inside the actuve area from the GPU.
		DownloadChunks(task_organizer);
	}
	else
		current_tick_fractional_= cur_tick_fractional;

	// Run player update independent on world update - every frame.
	// This is needed in order to make player movement and rotation smooth.
	UpdatePlayer(task_organizer, time_delta_s, keyboard_state, mouse_state, aspect);

	++current_frame_;
}

vk::Buffer WorldProcessor::GetChunkDataBuffer(const uint32_t index) const
{
	HEX_ASSERT(index < 2);
	return chunk_data_buffers_[index].GetBuffer();
}

vk::DeviceSize WorldProcessor::GetChunkDataBufferSize() const
{
	return chunk_data_buffers_[0].GetSize();
}

vk::Buffer WorldProcessor::GetChunkAuxiliarDataBuffer(const uint32_t index) const
{
	HEX_ASSERT(index < 2);
	return chunk_auxiliar_data_buffers_[index].GetBuffer();
}

vk::DeviceSize WorldProcessor::GetChunkAuxiliarDataBufferSize() const
{
	return chunk_auxiliar_data_buffers_[0].GetSize();
}

vk::Buffer WorldProcessor::GetLightDataBuffer(const uint32_t index) const
{
	HEX_ASSERT(index < 2);
	return light_buffers_[index].GetBuffer();
}

vk::DeviceSize WorldProcessor::GetLightDataBufferSize() const
{
	return light_buffers_[0].GetSize();
}

vk::Buffer WorldProcessor::GetPlayerStateBuffer() const
{
	return player_state_buffer_.GetBuffer();
}

vk::Buffer WorldProcessor::GetWorldGlobalStateBuffer() const
{
	return world_global_state_buffer_.GetBuffer();
}

WorldSizeChunks WorldProcessor::GetWorldSize() const
{
	return world_size_;
}

WorldOffsetChunks WorldProcessor::GetWorldOffset() const
{
	return world_offset_;
}

uint32_t WorldProcessor::GetActualBuffersIndex() const
{
	return GetSrcBufferIndex();
}

const WorldProcessor::PlayerState* WorldProcessor::GetLastKnownPlayerState() const
{
	return last_known_player_state_ == std::nullopt ? nullptr : &*last_known_player_state_;
}

void WorldProcessor::InitialFillBuffers(TaskOrganizer& task_organizer)
{
	if(initial_buffers_filled_)
		return;
	initial_buffers_filled_= true;

	TaskOrganizer::TransferTaskParams task;
	task.output_buffers.push_back(tree_map_buffer_.GetBuffer());
	task.output_buffers.push_back(chunk_gen_info_buffer_.GetBuffer());
	task.output_buffers.push_back(chunk_data_buffers_[0].GetBuffer());
	task.output_buffers.push_back(chunk_data_buffers_[1].GetBuffer());
	task.output_buffers.push_back(chunk_auxiliar_data_buffers_[0].GetBuffer());
	task.output_buffers.push_back(chunk_auxiliar_data_buffers_[1].GetBuffer());
	task.output_buffers.push_back(light_buffers_[0].GetBuffer());
	task.output_buffers.push_back(light_buffers_[1].GetBuffer());
	task.output_buffers.push_back(player_state_buffer_.GetBuffer());
	task.output_buffers.push_back(world_blocks_external_update_queue_buffer_.GetBuffer());
	task.output_buffers.push_back(player_world_window_buffer_.GetBuffer());
	task.output_buffers.push_back(player_state_read_back_buffer_.GetBuffer());
	task.output_buffers.push_back(world_global_state_buffer_.GetBuffer());

	const auto task_func=
		[this](const vk::CommandBuffer command_buffer)
		{
			{
				const TreeMap tree_map= GenTreeMap(world_seed_);
				command_buffer.updateBuffer(
					tree_map_buffer_.GetBuffer(),
					0,
					sizeof(TreeMap),
					static_cast<const void*>(&tree_map));
			}

			command_buffer.fillBuffer(chunk_gen_info_buffer_.GetBuffer(), 0, chunk_gen_info_buffer_.GetSize(), 0);

			for(uint32_t i= 0; i < 2; ++i)
			{
				// Zero chunk data buffers in order to prevent bugs with uninitialized memory.
				command_buffer.fillBuffer(chunk_data_buffers_[i].GetBuffer(), 0, chunk_data_buffers_[i].GetSize(), 0);
				command_buffer.fillBuffer(chunk_auxiliar_data_buffers_[i].GetBuffer(), 0, chunk_auxiliar_data_buffers_[i].GetSize(), 0);
				// Fill ight buffers with initial zeros.
				command_buffer.fillBuffer(light_buffers_[i].GetBuffer(), 0, light_buffers_[i].GetSize(), 0);
			}

			// Set initial player state.
			{
				PlayerState player_state;
				player_state.pos[0]= 0.0f;
				player_state.pos[1]= 0.0f;
				player_state.pos[2]= 40.0f;
				player_state.build_block_type= BlockType::Stone;

				command_buffer.updateBuffer(
					player_state_buffer_.GetBuffer(),
					0,
					sizeof(PlayerState),
					static_cast<const void*>(&player_state));
			}

			command_buffer.fillBuffer(world_blocks_external_update_queue_buffer_.GetBuffer(), 0, sizeof(WorldBlocksExternalUpdateQueue), 0);

			command_buffer.fillBuffer(player_world_window_buffer_.GetBuffer(), 0, sizeof(PlayerWorldWindow), 0);

			// Fill this buffer just to prevent some mistakes.
			command_buffer.fillBuffer(player_state_read_back_buffer_.GetBuffer(), 0, player_state_read_back_buffer_.GetSize(), 0);

			// Fill this buffer just to prevent some mistakes.
			command_buffer.fillBuffer(world_global_state_buffer_.GetBuffer(), 0, world_global_state_buffer_.GetSize(), 0);
		};

	task_organizer.ExecuteTask(task, task_func);
}

void WorldProcessor::ReadBackAndProcessPlayerState()
{
	// Assuming that writes into this buffer are finished in "player_state_read_back_buffer_num_frames_" frames.

	if(current_frame_ < player_state_read_back_buffer_num_frames_)
		return;

	const uint32_t current_slot= (current_frame_ - player_state_read_back_buffer_num_frames_) % player_state_read_back_buffer_num_frames_;

	last_known_player_state_.emplace();

	std::memcpy(
		&last_known_player_state_.value(),
		static_cast<const uint8_t*>(player_state_read_back_buffer_mapped_) + current_slot * sizeof(PlayerState),
		sizeof(PlayerState));

	// Log::Info("player pos: ", player_state.pos[0], ", ", player_state.pos[1], ", ", player_state.pos[2]);

	// Determine world position based on player position.

	// TODO - handle cases where player position changes a lot (like teleportation).

	const int32_t chunk_coord[]
	{
		int32_t(std::floor(last_known_player_state_->pos[0] / c_space_scale_x)) >> int32_t(c_chunk_width_log2),
		int32_t(std::floor(last_known_player_state_->pos[1])) >> int32_t(c_chunk_width_log2),
	};
	for(uint32_t i= 0; i < 2; ++i)
	{
		const int32_t chunk_relative_coord= chunk_coord[i] - next_next_world_offset_[i];

		const int32_t max_dist_to_border= std::max(0, int32_t(world_size_[i]) / 2 - 2);

		// Shift the world in single step in order to avoid mowing the world too much at once.
		if(chunk_relative_coord < max_dist_to_border)
			--next_next_world_offset_[i];
		else if(chunk_relative_coord >= int32_t(world_size_[i]) - max_dist_to_border)
			++next_next_world_offset_[i];
	}
}

void WorldProcessor::InitialFillWorld(TaskOrganizer& task_organizer)
{
	// Fill lists used by world generate/upload function.

	chunks_upate_kind_.clear();
	chunks_upate_kind_.resize(world_size_[0] * world_size_[1], ChunkUpdateKind::Generate);

	current_frame_chunks_to_update_list_.clear();

	for(uint32_t y= 0; y < world_size_[1]; ++y)
	for(uint32_t x= 0; x < world_size_[0]; ++x)
	{
		const uint32_t chunk_index= x + y * world_size_[0];

		if(chunks_storage_.GetChunk({int32_t(x) + world_offset_[0], int32_t(y) + world_offset_[1]}) != nullptr)
			chunks_upate_kind_[chunk_index]= ChunkUpdateKind::Upload;
		else
			chunks_upate_kind_[chunk_index]= ChunkUpdateKind::Generate;

		current_frame_chunks_to_update_list_.push_back({x, y});
	}

	// Trigger world generation - for chunks which do not exist in the storage.
	GenerateWorld(task_organizer, {0, 0});

	// Upload chunks existing in the storage.
	UploadChunks(task_organizer);
}

void WorldProcessor::DetermineChunksUpdateKind(const RelativeWorldShiftChunks relative_world_shift)
{
	chunks_upate_kind_.resize(world_size_[0] * world_size_[1], ChunkUpdateKind::Update);

	for(uint32_t y= 0; y < world_size_[1]; ++y)
	for(uint32_t x= 0; x < world_size_[0]; ++x)
	{
		const uint32_t chunk_index= x + y * world_size_[0];

		const int32_t in_position[]
		{
			int32_t(x) + relative_world_shift[0],
			int32_t(y) + relative_world_shift[1],
		};

		if( in_position[0] >= 0 && in_position[0] < int32_t(world_size_[0]) &&
			in_position[1] >= 0 && in_position[1] < int32_t(world_size_[1]))
			chunks_upate_kind_[chunk_index]= ChunkUpdateKind::Update;
		else
		{
			if(chunks_storage_.GetChunk({
				int32_t(x) + world_offset_[0] + int32_t(relative_world_shift[0]),
				int32_t(y) + world_offset_[1] + int32_t(relative_world_shift[1])}) != nullptr)
				chunks_upate_kind_[chunk_index]= ChunkUpdateKind::Upload;
			else
				chunks_upate_kind_[chunk_index]= ChunkUpdateKind::Generate;
		}
	}
}

void WorldProcessor::BuildCurrentFrameChunksToUpdateList(
	const float prev_offset_within_tick,
	const float cur_offset_within_tick)
{
	// Split update of world blocks and lighting evenly between multiple frames of the same tick.

	HEX_ASSERT(prev_offset_within_tick >= 0.0f);
	HEX_ASSERT(cur_offset_within_tick <= 1.0f);
	HEX_ASSERT(prev_offset_within_tick <= cur_offset_within_tick);

	current_frame_chunks_to_update_list_.clear();

	const uint32_t total_chunks= world_size_[0] * world_size_[1];
	const uint32_t start_chunk_index= uint32_t(float(total_chunks) * prev_offset_within_tick);
	const uint32_t end_chunk_index= std::min(total_chunks, uint32_t(float(total_chunks) * cur_offset_within_tick));

	for(uint32_t chunk_index= start_chunk_index; chunk_index < end_chunk_index; ++chunk_index)
		current_frame_chunks_to_update_list_.push_back({chunk_index % world_size_[0], chunk_index / world_size_[0]});
}

void WorldProcessor::UpdateWorldGlobalState(TaskOrganizer& task_organizer, const DebugParams& debug_params)
{
	TaskOrganizer::ComputeTaskParams task;
	task.input_output_storage_buffers.push_back(world_global_state_buffer_.GetBuffer());

	const auto task_func=
		[this, &debug_params](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *world_global_state_update_pipeline_.pipeline);

			WorldGlobalStateUpdateUniforms uniforms;
			uniforms.time_of_day= debug_params.time_of_day;

			command_buffer.pushConstants(
				*world_global_state_update_pipeline_.pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				sizeof(WorldGlobalStateUpdateUniforms), static_cast<const void*>(&uniforms));

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*world_global_state_update_pipeline_.pipeline_layout,
				0u,
				{world_global_state_update_descriptor_set_},
				{});

			// Distpatch single thread for world global state update.
			command_buffer.dispatch(1, 1, 1);
		};

	task_organizer.ExecuteTask(task, task_func);
}

void WorldProcessor::UpdateWorldBlocks(
	TaskOrganizer& task_organizer,
	const RelativeWorldShiftChunks relative_world_shift)
{
	const uint32_t src_buffer_index= GetSrcBufferIndex();
	const uint32_t dst_buffer_index= GetDstBufferIndex();

	TaskOrganizer::ComputeTaskParams task;
	task.input_storage_buffers.push_back(chunk_data_buffers_[src_buffer_index].GetBuffer());
	task.input_storage_buffers.push_back(chunk_auxiliar_data_buffers_[src_buffer_index].GetBuffer());
	task.output_storage_buffers.push_back(chunk_data_buffers_[dst_buffer_index].GetBuffer());
	task.output_storage_buffers.push_back(chunk_auxiliar_data_buffers_[dst_buffer_index].GetBuffer());

	const auto task_func=
		[this, src_buffer_index, relative_world_shift](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *world_blocks_update_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*world_blocks_update_pipeline_.pipeline_layout,
				0u,
				{world_blocks_update_descriptor_sets_[src_buffer_index]},
				{});

			for(const auto& chunk_to_update : current_frame_chunks_to_update_list_)
			{
				const uint32_t chunk_index= chunk_to_update[0] + chunk_to_update[1] * world_size_[0];
				if(chunks_upate_kind_[chunk_index] != ChunkUpdateKind::Update)
					continue;

				WorldBlocksUpdateUniforms uniforms;
				uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
				uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
				uniforms.in_chunk_position[0]= int32_t(chunk_to_update[0]) + relative_world_shift[0];
				uniforms.in_chunk_position[1]= int32_t(chunk_to_update[1]) + relative_world_shift[1];
				uniforms.out_chunk_position[0]= int32_t(chunk_to_update[0]);
				uniforms.out_chunk_position[1]= int32_t(chunk_to_update[1]);
				uniforms.current_tick= current_tick_;

				HEX_ASSERT(uniforms.in_chunk_position[0] >= 0 && uniforms.in_chunk_position[0] < int32_t(world_size_[0]));
				HEX_ASSERT(uniforms.in_chunk_position[1] >= 0 && uniforms.in_chunk_position[1] < int32_t(world_size_[1]));
				HEX_ASSERT(uniforms.out_chunk_position[0] >= 0 && uniforms.out_chunk_position[0] < int32_t(world_size_[0]));
				HEX_ASSERT(uniforms.out_chunk_position[1] >= 0 && uniforms.out_chunk_position[1] < int32_t(world_size_[1]));

				command_buffer.pushConstants(
					*world_blocks_update_pipeline_.pipeline_layout,
					vk::ShaderStageFlagBits::eCompute,
					0,
					sizeof(WorldBlocksUpdateUniforms), static_cast<const void*>(&uniforms));

				// This constant should match workgroup size in shader!
				constexpr uint32_t c_workgroup_size[]{4, 4, 8};
				static_assert(c_chunk_width % c_workgroup_size[0] == 0, "Wrong workgroup size!");
				static_assert(c_chunk_width % c_workgroup_size[1] == 0, "Wrong workgroup size!");
				static_assert(c_chunk_height % c_workgroup_size[2] == 0, "Wrong workgroup size!");

				command_buffer.dispatch(
					c_chunk_width / c_workgroup_size[0],
					c_chunk_width / c_workgroup_size[1],
					c_chunk_height / c_workgroup_size[2]);
			}
		};

	task_organizer.ExecuteTask(task, task_func);
}

void WorldProcessor::UpdateLight(
	TaskOrganizer& task_organizer,
	const RelativeWorldShiftChunks relative_world_shift)
{
	const uint32_t src_buffer_index= GetSrcBufferIndex();
	const uint32_t dst_buffer_index= GetDstBufferIndex();

	TaskOrganizer::ComputeTaskParams task;
	task.input_storage_buffers.push_back(chunk_data_buffers_[src_buffer_index].GetBuffer());
	task.input_storage_buffers.push_back(light_buffers_[src_buffer_index].GetBuffer());
	task.output_storage_buffers.push_back(light_buffers_[dst_buffer_index].GetBuffer());

	const auto task_func=
		[this, src_buffer_index, relative_world_shift](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *light_update_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*light_update_pipeline_.pipeline_layout,
				0u,
				{light_update_descriptor_sets_[src_buffer_index]},
				{});

			for(const auto& chunk_to_update : current_frame_chunks_to_update_list_)
			{
				const uint32_t chunk_index= chunk_to_update[0] + chunk_to_update[1] * world_size_[0];
				if(chunks_upate_kind_[chunk_index] != ChunkUpdateKind::Update)
					continue;

				LightUpdateUniforms uniforms;
				uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
				uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
				uniforms.in_chunk_position[0]= int32_t(chunk_to_update[0]) + relative_world_shift[0];
				uniforms.in_chunk_position[1]= int32_t(chunk_to_update[1]) + relative_world_shift[1];
				uniforms.out_chunk_position[0]= int32_t(chunk_to_update[0]);
				uniforms.out_chunk_position[1]= int32_t(chunk_to_update[1]);

				HEX_ASSERT(uniforms.in_chunk_position[0] >= 0 && uniforms.in_chunk_position[0] < int32_t(world_size_[0]));
				HEX_ASSERT(uniforms.in_chunk_position[1] >= 0 && uniforms.in_chunk_position[1] < int32_t(world_size_[1]));
				HEX_ASSERT(uniforms.out_chunk_position[0] >= 0 && uniforms.out_chunk_position[0] < int32_t(world_size_[0]));
				HEX_ASSERT(uniforms.out_chunk_position[1] >= 0 && uniforms.out_chunk_position[1] < int32_t(world_size_[1]));

				command_buffer.pushConstants(
					*light_update_pipeline_.pipeline_layout,
					vk::ShaderStageFlagBits::eCompute,
					0,
					sizeof(LightUpdateUniforms), static_cast<const void*>(&uniforms));

				// This constant should match workgroup size in shader!
				constexpr uint32_t c_workgroup_size[]{4, 4, 8};
				static_assert(c_chunk_width % c_workgroup_size[0] == 0, "Wrong workgroup size!");
				static_assert(c_chunk_width % c_workgroup_size[1] == 0, "Wrong workgroup size!");
				static_assert(c_chunk_height % c_workgroup_size[2] == 0, "Wrong workgroup size!");

				command_buffer.dispatch(
					c_chunk_width / c_workgroup_size[0],
					c_chunk_width / c_workgroup_size[1],
					c_chunk_height / c_workgroup_size[2]);
			}
		};

	task_organizer.ExecuteTask(task, task_func);
}

void WorldProcessor::GenerateWorld(
	TaskOrganizer& task_organizer,
	const RelativeWorldShiftChunks relative_world_shift)
{
	TaskOrganizer::ComputeTaskParams chunk_gen_prepare_task;
	chunk_gen_prepare_task.input_storage_buffers.push_back(structures_buffer_.GetDescriptionsBuffer());
	chunk_gen_prepare_task.input_storage_buffers.push_back(tree_map_buffer_.GetBuffer());
	chunk_gen_prepare_task.output_storage_buffers.push_back(chunk_gen_info_buffer_.GetBuffer());

	const auto chunk_gen_prepare_task_func=
		[this, relative_world_shift](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *chunk_gen_prepare_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*chunk_gen_prepare_pipeline_.pipeline_layout,
				0u,
				{chunk_gen_prepare_descriptor_set_},
				{});

			for(const auto& chunk_to_update : current_frame_chunks_to_update_list_)
			{
				const uint32_t chunk_index= chunk_to_update[0] + chunk_to_update[1] * world_size_[0];
				if(chunks_upate_kind_[chunk_index] != ChunkUpdateKind::Generate)
					continue;

				ChunkGenPrepareUniforms uniforms;
				uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
				uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
				uniforms.chunk_position[0]= int32_t(chunk_to_update[0]);
				uniforms.chunk_position[1]= int32_t(chunk_to_update[1]);
				uniforms.chunk_global_position[0]= world_offset_[0] + int32_t(chunk_to_update[0]) + relative_world_shift[0];
				uniforms.chunk_global_position[1]= world_offset_[1] + int32_t(chunk_to_update[1]) + relative_world_shift[1];
				uniforms.seed= world_seed_;

				command_buffer.pushConstants(
					*chunk_gen_prepare_pipeline_.pipeline_layout,
					vk::ShaderStageFlagBits::eCompute,
					0,
					sizeof(ChunkGenPrepareUniforms), static_cast<const void*>(&uniforms));

				// For now perform single-threaded preparation.
				command_buffer.dispatch(1, 1, 1);
			}
		};

	task_organizer.ExecuteTask(chunk_gen_prepare_task, chunk_gen_prepare_task_func);

	const uint32_t dst_buffer_index= GetDstBufferIndex();

	TaskOrganizer::ComputeTaskParams world_gen_task;
	world_gen_task.input_storage_buffers.push_back(chunk_gen_info_buffer_.GetBuffer());
	world_gen_task.input_storage_buffers.push_back(structures_buffer_.GetDescriptionsBuffer());
	world_gen_task.input_storage_buffers.push_back(structures_buffer_.GetDataBuffer());
	world_gen_task.output_storage_buffers.push_back(chunk_data_buffers_[dst_buffer_index].GetBuffer());
	world_gen_task.output_storage_buffers.push_back(chunk_auxiliar_data_buffers_[dst_buffer_index].GetBuffer());

	const auto world_gen_task_func=
		[this, dst_buffer_index, relative_world_shift](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *world_gen_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*world_gen_pipeline_.pipeline_layout,
				0u,
				{world_gen_descriptor_sets_[dst_buffer_index]},
				{});

			for(const auto& chunk_to_update : current_frame_chunks_to_update_list_)
			{
				const uint32_t chunk_index= chunk_to_update[0] + chunk_to_update[1] * world_size_[0];
				if(chunks_upate_kind_[chunk_index] != ChunkUpdateKind::Generate)
					continue;

				WorldGenUniforms uniforms;
				uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
				uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
				uniforms.chunk_position[0]= int32_t(chunk_to_update[0]);
				uniforms.chunk_position[1]= int32_t(chunk_to_update[1]);
				uniforms.chunk_global_position[0]= world_offset_[0] + int32_t(chunk_to_update[0]) + relative_world_shift[0];
				uniforms.chunk_global_position[1]= world_offset_[1] + int32_t(chunk_to_update[1]) + relative_world_shift[1];
				uniforms.seed= world_seed_;

				command_buffer.pushConstants(
					*world_gen_pipeline_.pipeline_layout,
					vk::ShaderStageFlagBits::eCompute,
					0,
					sizeof(WorldGenUniforms), static_cast<const void*>(&uniforms));

				// Dispatch only 2D group - perform generation for columns.
				command_buffer.dispatch(
					c_chunk_width / c_world_gen_workgroup_size[0],
					c_chunk_width / c_world_gen_workgroup_size[1],
					1);
			}
		};

	task_organizer.ExecuteTask(world_gen_task, world_gen_task_func);

	TaskOrganizer::ComputeTaskParams initial_light_fill_task;
	initial_light_fill_task.input_storage_buffers.push_back(chunk_data_buffers_[dst_buffer_index].GetBuffer());
	initial_light_fill_task.output_storage_buffers.push_back(light_buffers_[dst_buffer_index].GetBuffer());

	const auto initial_light_fill_task_func=
		[this, dst_buffer_index](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *initial_light_fill_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*initial_light_fill_pipeline_.pipeline_layout,
				0u,
				{initial_light_fill_descriptor_sets_[dst_buffer_index]},
				{});

			for(const auto& chunk_to_update : current_frame_chunks_to_update_list_)
			{
				const uint32_t chunk_index= chunk_to_update[0] + chunk_to_update[1] * world_size_[0];
				if(chunks_upate_kind_[chunk_index] != ChunkUpdateKind::Generate)
					continue;

				InitialLightFillUniforms uniforms;
				uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
				uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
				uniforms.chunk_position[0]= int32_t(chunk_to_update[0]);
				uniforms.chunk_position[1]= int32_t(chunk_to_update[1]);

				command_buffer.pushConstants(
					*initial_light_fill_pipeline_.pipeline_layout,
					vk::ShaderStageFlagBits::eCompute,
					0,
					sizeof(InitialLightFillUniforms), static_cast<const void*>(&uniforms));

				// Dispatch only 2D group - perform light fill for columns.
				command_buffer.dispatch(
					c_chunk_width / c_initial_light_fill_workgroup_size[0],
					c_chunk_width / c_initial_light_fill_workgroup_size[1],
					1);
			}
		};

	task_organizer.ExecuteTask(initial_light_fill_task, initial_light_fill_task_func);
}

void WorldProcessor::DownloadChunks(TaskOrganizer& task_organizer)
{
	if(world_offset_ == next_world_offset_)
		return;

	const RelativeWorldShiftChunks relative_shift
	{
		next_world_offset_[0] - world_offset_[0],
		next_world_offset_[1] - world_offset_[1],
	};

	const uint32_t src_buffer_index= GetSrcBufferIndex();

	TaskOrganizer::TransferTaskParams task;
	task.input_buffers.push_back(chunk_data_buffers_[src_buffer_index].GetBuffer());
	task.input_buffers.push_back(chunk_auxiliar_data_buffers_[src_buffer_index].GetBuffer());
	task.output_buffers.push_back(chunk_data_load_buffer_.GetBuffer());
	task.output_buffers.push_back(chunk_auxiliar_data_load_buffer_.GetBuffer());

	const auto task_func=
		[this, relative_shift, src_buffer_index](const vk::CommandBuffer command_buffer)
		{
			for(uint32_t y= 0; y < world_size_[1]; ++y)
			for(uint32_t x= 0; x < world_size_[0]; ++x)
			{
				const int32_t old_pos[2]
				{
					int32_t(x) - relative_shift[0],
					int32_t(y) - relative_shift[1],
				};

				if( old_pos[0] < 0 || old_pos[0] >= int32_t(world_size_[0]) ||
					old_pos[1] < 0 || old_pos[1] >= int32_t(world_size_[1]))
				{
					// Need to download this chunk.
					const uint32_t chunk_index= x + y * world_size_[0];
					const uint32_t offset= chunk_index * c_chunk_volume;

					command_buffer.copyBuffer(
						chunk_data_buffers_[src_buffer_index].GetBuffer(),
						chunk_data_load_buffer_.GetBuffer(),
						{ { offset, offset, c_chunk_volume }});

					command_buffer.copyBuffer(
						chunk_auxiliar_data_buffers_[src_buffer_index].GetBuffer(),
						chunk_auxiliar_data_load_buffer_.GetBuffer(),
						{ { offset, offset, c_chunk_volume }});
				}
			}

			// Set the event at the end of transfer commands, to tell host that data is available.
			// TODO - make sure this works properly
			// and all data written into the buffer may be accessed by the host after this event is signaled.
			command_buffer.setEvent(*chunk_data_download_event_, vk::PipelineStageFlagBits::eTransfer);
			wait_for_chunks_data_download_= true;
		};

	task_organizer.ExecuteTask(task, task_func);
}

void WorldProcessor::FinishChunksDownloading(TaskOrganizer& task_organizer)
{
	if(!wait_for_chunks_data_download_)
		return;

	if(vk_device_.getEventStatus(*chunk_data_download_event_) != vk::Result::eEventSet)
		return; // Not finished yet.

	// GPU-side chunks data copying is finished - reset the event and process data obtained.
	vk_device_.resetEvent(*chunk_data_download_event_);
	wait_for_chunks_data_download_= false;

	const RelativeWorldShiftChunks relative_shift
	{
		next_world_offset_[0] - world_offset_[0],
		next_world_offset_[1] - world_offset_[1],
	};

	// Invalidate host caches for buffers memory before writing. This is needed for non-coherent memory.
	std::vector<vk::MappedMemoryRange> mapped_memory_ranges;
	for(uint32_t y= 0; y < world_size_[1]; ++y)
	for(uint32_t x= 0; x < world_size_[0]; ++x)
	{
		const int32_t old_pos[2]
		{
			int32_t(x) - relative_shift[0],
			int32_t(y) - relative_shift[1],
		};

		if( old_pos[0] < 0 || old_pos[0] >= int32_t(world_size_[0]) ||
			old_pos[1] < 0 || old_pos[1] >= int32_t(world_size_[1]))
		{
			const uint32_t chunk_index= x + y * world_size_[0];
			const uint32_t offset= chunk_index * c_chunk_volume;

			mapped_memory_ranges.emplace_back(
				chunk_data_load_buffer_.GetMemory(), offset, c_chunk_volume);

			mapped_memory_ranges.emplace_back(
				chunk_auxiliar_data_load_buffer_.GetMemory(), offset, c_chunk_volume);
		}
	}
	vk_device_.invalidateMappedMemoryRanges(mapped_memory_ranges);

	// Compress and save downloaded chunks into the storage.
	// TODO - make this in background thread?
	for(uint32_t y= 0; y < world_size_[1]; ++y)
	for(uint32_t x= 0; x < world_size_[0]; ++x)
	{
		const int32_t old_pos[2]
		{
			int32_t(x) - relative_shift[0],
			int32_t(y) - relative_shift[1],
		};

		if( old_pos[0] < 0 || old_pos[0] >= int32_t(world_size_[0]) ||
			old_pos[1] < 0 || old_pos[1] >= int32_t(world_size_[1]))
		{
			const uint32_t chunk_index= x + y * world_size_[0];
			const uint32_t offset= chunk_index * c_chunk_volume;

			chunks_storage_.SetChunk(
				{
					int32_t(x) + world_offset_[0],
					int32_t(y) + world_offset_[1]
				},
				chunk_data_compressor_.Compress(
					static_cast<const BlockType*>(chunk_data_load_buffer_mapped_) + offset,
					static_cast<const uint8_t*>(chunk_auxiliar_data_load_buffer_mapped_) + offset));
		}
	}

	// Now we can upload chunks.
	UploadChunks(task_organizer);
}

void WorldProcessor::UploadChunks(TaskOrganizer& task_organizer)
{
	// Decompress chunks first.
	// TODO - make this in background thread?
	std::vector<vk::MappedMemoryRange> written_mapped_memory_ranges;
	for(uint32_t y= 0; y < world_size_[1]; ++y)
	for(uint32_t x= 0; x < world_size_[0]; ++x)
	{
		const uint32_t chunk_index= x + y * world_size_[0];
		if(chunks_upate_kind_[chunk_index] == ChunkUpdateKind::Upload)
		{
			const uint32_t offset= chunk_index * c_chunk_volume;

			const ChunkDataCompresed* const chunk_data_compressed= chunks_storage_.GetChunk(
				{
					int32_t(x) + next_world_offset_[0],
					int32_t(y) + next_world_offset_[1]
				});

			HEX_ASSERT(chunk_data_compressed != nullptr);
			chunk_data_compressor_.Decompress(
				*chunk_data_compressed,
				static_cast<BlockType*>(chunk_data_load_buffer_mapped_) + offset,
				static_cast<uint8_t*>(chunk_auxiliar_data_load_buffer_mapped_) + offset);

			written_mapped_memory_ranges.emplace_back(
				chunk_data_load_buffer_.GetMemory(), offset, c_chunk_volume);

			written_mapped_memory_ranges.emplace_back(
				chunk_auxiliar_data_load_buffer_.GetMemory(), offset, c_chunk_volume);
		}
	}

	// Flush host writes into buffers memory. This is needed for non-coherent memory.
	vk_device_.flushMappedMemoryRanges(written_mapped_memory_ranges);

	// Schedule copying data from loading buffers to actual buffers.

	const uint32_t dst_buffer_index= GetDstBufferIndex();

	TaskOrganizer::TransferTaskParams task;
	task.input_buffers.push_back(chunk_data_load_buffer_.GetBuffer());
	task.input_buffers.push_back(chunk_auxiliar_data_load_buffer_.GetBuffer());
	task.output_buffers.push_back(chunk_data_buffers_[dst_buffer_index].GetBuffer());
	task.output_buffers.push_back(chunk_auxiliar_data_buffers_[dst_buffer_index].GetBuffer());

	const auto task_func=
		[this, dst_buffer_index](const vk::CommandBuffer command_buffer)
		{
			for(uint32_t y= 0; y < world_size_[1]; ++y)
			for(uint32_t x= 0; x < world_size_[0]; ++x)
			{
				const uint32_t chunk_index= x + y * world_size_[0];
				if(chunks_upate_kind_[chunk_index] == ChunkUpdateKind::Upload)
				{
					const uint32_t offset= chunk_index * c_chunk_volume;

					command_buffer.copyBuffer(
						chunk_data_load_buffer_.GetBuffer(),
						chunk_data_buffers_[dst_buffer_index].GetBuffer(),
						{ { offset, offset, c_chunk_volume }});

					command_buffer.copyBuffer(
						chunk_auxiliar_data_load_buffer_.GetBuffer(),
						chunk_auxiliar_data_buffers_[dst_buffer_index].GetBuffer(),
						{ { offset, offset, c_chunk_volume }});
				}
			}
		};

	task_organizer.ExecuteTask(task, task_func);

	// Perform initial light fill for loaded chunks.
	TaskOrganizer::ComputeTaskParams initial_light_fill_task;
	initial_light_fill_task.input_storage_buffers.push_back(chunk_data_buffers_[dst_buffer_index].GetBuffer());
	initial_light_fill_task.output_storage_buffers.push_back(light_buffers_[dst_buffer_index].GetBuffer());

	const auto initial_light_fill_task_func=
		[this, dst_buffer_index](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *initial_light_fill_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*initial_light_fill_pipeline_.pipeline_layout,
				0u,
				{initial_light_fill_descriptor_sets_[dst_buffer_index]},
				{});

			for(uint32_t y= 0; y < world_size_[1]; ++y)
			for(uint32_t x= 0; x < world_size_[0]; ++x)
			{
				const uint32_t chunk_index= x + y * world_size_[0];
				if(chunks_upate_kind_[chunk_index] == ChunkUpdateKind::Upload)
				{
					InitialLightFillUniforms uniforms;
					uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
					uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
					uniforms.chunk_position[0]= int32_t(x);
					uniforms.chunk_position[1]= int32_t(y);

					command_buffer.pushConstants(
						*initial_light_fill_pipeline_.pipeline_layout,
						vk::ShaderStageFlagBits::eCompute,
						0,
						sizeof(InitialLightFillUniforms), static_cast<const void*>(&uniforms));

					// Dispatch only 2D group - perform light fill for columns.
					command_buffer.dispatch(
						c_chunk_width / c_initial_light_fill_workgroup_size[0],
						c_chunk_width / c_initial_light_fill_workgroup_size[1],
						1);
				}
			}
		};

	task_organizer.ExecuteTask(initial_light_fill_task, initial_light_fill_task_func);
}

void WorldProcessor::BuildPlayerWorldWindow(TaskOrganizer& task_organizer)
{
	const uint32_t src_buffer_index= GetSrcBufferIndex();

	TaskOrganizer::ComputeTaskParams task;
	task.input_storage_buffers.push_back(player_state_buffer_.GetBuffer());
	task.input_storage_buffers.push_back(chunk_data_buffers_[src_buffer_index].GetBuffer());
	task.output_storage_buffers.push_back(player_world_window_buffer_.GetBuffer());

	const auto task_func=
		[this, src_buffer_index](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *player_world_window_build_pipeline_.pipeline);

			// Build player world window based on src world state.
			const auto descriptor_set= player_world_window_build_descriptor_sets_[src_buffer_index];

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*player_world_window_build_pipeline_.pipeline_layout,
				0u,
				{descriptor_set},
				{});

			PlayerWorldWindowBuildUniforms uniforms;
			uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
			uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
			uniforms.world_offset_chunks[0]= world_offset_[0];
			uniforms.world_offset_chunks[1]= world_offset_[1];

			command_buffer.pushConstants(
				*player_world_window_build_pipeline_.pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				sizeof(PlayerWorldWindowBuildUniforms), static_cast<const void*>(&uniforms));

			// This constant should match workgroup size in shader!
			constexpr uint32_t c_workgroup_size[]{4, 4, 8};
			static_assert(c_player_world_window_size[0] % c_workgroup_size[0] == 0, "Wrong workgroup size!");
			static_assert(c_player_world_window_size[1] % c_workgroup_size[1] == 0, "Wrong workgroup size!");
			static_assert(c_player_world_window_size[2] % c_workgroup_size[2] == 0, "Wrong workgroup size!");

			command_buffer.dispatch(
				c_player_world_window_size[0] / c_workgroup_size[0],
				c_player_world_window_size[1] / c_workgroup_size[1],
				c_player_world_window_size[2] / c_workgroup_size[2]);
		};

	task_organizer.ExecuteTask(task, task_func);
}

void WorldProcessor::UpdatePlayer(
	TaskOrganizer& task_organizer,
	const float time_delta_s,
	const KeyboardState keyboard_state,
	const MouseState mouse_state,
	const float aspect)
{
	PlayerUpdateUniforms player_update_uniforms;
	player_update_uniforms.aspect= aspect;
	player_update_uniforms.time_delta_s= time_delta_s;
	player_update_uniforms.keyboard_state= keyboard_state;
	player_update_uniforms.mouse_state= mouse_state;

	TaskOrganizer::ComputeTaskParams player_update_task;
	player_update_task.input_output_storage_buffers.push_back(player_state_buffer_.GetBuffer());
	player_update_task.input_output_storage_buffers.push_back(world_blocks_external_update_queue_buffer_.GetBuffer());
	player_update_task.input_output_storage_buffers.push_back(player_world_window_buffer_.GetBuffer());

	const auto player_update_task_func=
		[this, player_update_uniforms](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *player_update_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*player_update_pipeline_.pipeline_layout,
				0u,
				{player_update_descriptor_set_},
				{});

			command_buffer.pushConstants(
				*player_update_pipeline_.pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				sizeof(PlayerUpdateUniforms), static_cast<const void*>(&player_update_uniforms));

			// Player update is single-threaded.
			command_buffer.dispatch(1, 1, 1);
		};

	task_organizer.ExecuteTask(player_update_task, player_update_task_func);

	TaskOrganizer::TransferTaskParams player_state_read_back_task;
	player_state_read_back_task.input_buffers.push_back(player_state_buffer_.GetBuffer());
	player_state_read_back_task.output_buffers.push_back(player_state_read_back_buffer_.GetBuffer());

	const auto player_state_read_back_task_func=
		[this](const vk::CommandBuffer command_buffer)
		{
			// Copy player state into readback buffer.
			// Use slot in the destination buffer for this frame.
			command_buffer.copyBuffer(
				player_state_buffer_.GetBuffer(),
				player_state_read_back_buffer_.GetBuffer(),
				{
					{
						0,
						sizeof(PlayerState) * (current_frame_ % player_state_read_back_buffer_num_frames_),
						sizeof(PlayerState)
					}
				});
		};

	task_organizer.ExecuteTask(player_state_read_back_task, player_state_read_back_task_func);
}

void WorldProcessor::FlushWorldBlocksExternalUpdateQueue(TaskOrganizer& task_organizer)
{
	// Flush the queue into the destination world buffer.
	const uint32_t dst_buffer_index= GetDstBufferIndex();

	TaskOrganizer::ComputeTaskParams task;
	task.input_output_storage_buffers.push_back(world_blocks_external_update_queue_buffer_.GetBuffer());
	task.input_output_storage_buffers.push_back(chunk_data_buffers_[dst_buffer_index].GetBuffer());
	task.input_output_storage_buffers.push_back(chunk_auxiliar_data_buffers_[dst_buffer_index].GetBuffer());

	const auto task_func=
		[this, dst_buffer_index](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *world_blocks_external_update_queue_flush_pipeline_.pipeline);

			// Flush the queue into the destination world buffer.
			const auto descriptor_set= world_blocks_external_update_queue_flush_descriptor_sets_[dst_buffer_index];

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*world_blocks_external_update_queue_flush_pipeline_.pipeline_layout,
				0u,
				{descriptor_set},
				{});

			WorldBlocksExternalUpdateQueueFlushUniforms uniforms;
			uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
			uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
			uniforms.world_offset_chunks[0]= world_offset_[0];
			uniforms.world_offset_chunks[1]= world_offset_[1];

			command_buffer.pushConstants(
				*world_blocks_external_update_queue_flush_pipeline_.pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				sizeof(WorldBlocksExternalUpdateQueueFlushUniforms), static_cast<const void*>(&uniforms));

			// Queue flushing is single-threaded.
			command_buffer.dispatch(1, 1, 1);
		};

	task_organizer.ExecuteTask(task, task_func);
}

uint32_t WorldProcessor::GetSrcBufferIndex() const
{
	return current_tick_ & 1;
}

uint32_t WorldProcessor::GetDstBufferIndex() const
{
	return GetSrcBufferIndex() ^ 1;
}

} // namespace HexGPU
