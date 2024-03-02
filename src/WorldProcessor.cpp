#include "WorldProcessor.hpp"
#include "Assert.hpp"
#include "Constants.hpp"
#include "Log.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

namespace WorldGenShaderBindings
{

uint32_t chunk_data_buffer= 0;

}

namespace InitialLightFillShaderBindings
{

uint32_t chunk_data_buffer= 0;
uint32_t light_data_buffer= 1;

}

namespace WorldBlocksUpdateShaderBindings
{

uint32_t chunk_data_input_buffer= 0;
uint32_t chunk_data_output_buffer= 1;

}

namespace LightUpdateShaderBindings
{

uint32_t chunk_data_buffer= 0;
uint32_t chunk_input_light_buffer= 1;
uint32_t chunk_output_light_buffer= 2;

}

namespace PlayerWorldWindowBuildShaderBindings
{

uint32_t chunk_data_buffer= 0;
uint32_t player_world_window_buffer= 1;
uint32_t player_state_buffer= 2;

}

namespace PlayerUpdateShaderBindings
{

uint32_t player_state_buffer= 1;
uint32_t world_blocks_external_update_queue_buffer= 2;
uint32_t player_world_window_buffer= 3;

}

namespace WorldBlocksExternalUpdateQueueFlushBindigns
{

uint32_t chunk_data_buffer= 0;
uint32_t world_blocks_external_update_queue_buffer= 1;

}

// Size limit of this struct is 128 bytes.
// 128 bytes is guaranted maximum size of push constants uniform block.
struct ChunkPositionUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t chunk_position[2]{}; // Position relative current loaded region.
	int32_t chunk_global_position[2]{}; // Global position.
};

struct PlayerWorldWindowBuildUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t world_offset_chunks[2]{};
};

struct PlayerUpdateUniforms
{
	int32_t world_size_chunks[2]{0, 0};
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

WorldSizeChunks ReadWorldSize(Settings& settings)
{
	const int32_t world_size_x= std::max(2, std::min(int32_t(settings.GetInt("g_world_size_x", 8)), 32));
	const int32_t world_size_y= std::max(2, std::min(int32_t(settings.GetInt("g_world_size_y", 8)), 32));

	settings.SetInt("g_world_size_x", world_size_x);
	settings.SetInt("g_world_size_y", world_size_y);

	Log::Info("World size is ", world_size_x, "x", world_size_y, " chunks");

	return WorldSizeChunks{uint32_t(world_size_x), uint32_t(world_size_y)};
}

} // namespace

WorldProcessor::WorldProcessor(
	WindowVulkan& window_vulkan,
	const vk::DescriptorPool global_descriptor_pool,
	Settings& settings)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_size_(ReadWorldSize(settings))
	, world_offset_{-int32_t(world_size_[0] / 2u), -int32_t(world_size_[1] / 2u)}
{
	// Create chunk data buffers.
	chunk_data_buffer_size_= c_chunk_volume * world_size_[0] * world_size_[1];
	for(uint32_t i= 0; i < 2; ++i)
	{
		chunk_data_buffers_[i].buffer=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					chunk_data_buffer_size_,
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*chunk_data_buffers_[i].buffer);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= i;
				break;
			}
		}

		chunk_data_buffers_[i].memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*chunk_data_buffers_[i].buffer, *chunk_data_buffers_[i].memory, 0u);
	}

	// Create light buffers.
	light_buffer_size_= c_chunk_volume * world_size_[0] * world_size_[1];
	for(uint32_t i= 0; i < 2; ++i)
	{
		light_buffers_[i].buffer=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					light_buffer_size_,
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*light_buffers_[i].buffer);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= i;
				break;
			}
		}

		light_buffers_[i].memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*light_buffers_[i].buffer, *light_buffers_[i].memory, 0u);
	}

	// Create player state buffer.
	{
		player_state_buffer_.buffer=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					sizeof(PlayerState),
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*player_state_buffer_.buffer);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= i;
				break;
			}
		}

		player_state_buffer_.memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*player_state_buffer_.buffer, *player_state_buffer_.memory, 0u);
	}

	// Create world blocks external update queue buffer.
	{
		world_blocks_external_update_queue_buffer_.buffer=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					sizeof(WorldBlocksExternalUpdateQueue),
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*world_blocks_external_update_queue_buffer_.buffer);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= i;
				break;
			}
		}

		world_blocks_external_update_queue_buffer_.memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*world_blocks_external_update_queue_buffer_.buffer, *world_blocks_external_update_queue_buffer_.memory, 0u);
	}

	// Create player world window buffer.
	{
		player_world_window_buffer_.buffer=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					sizeof(PlayerWorldWindow),
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*player_world_window_buffer_.buffer);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= i;
				break;
			}
		}

		player_world_window_buffer_.memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*player_world_window_buffer_.buffer, *player_world_window_buffer_.memory, 0u);
	}

	// Create world generation shader.
	world_gen_shader_= CreateShader(vk_device_, ShaderNames::world_gen_comp);

	// Create world generation descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				WorldGenShaderBindings::chunk_data_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
		};
		world_gen_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create world generation pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(ChunkPositionUniforms));

		world_gen_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*world_gen_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create world generation pipeline.
	world_gen_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*world_gen_shader_,
				"main"),
			*world_gen_pipeline_layout_)));


	// Create and update world generation descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		world_gen_descriptor_sets_[i]= vk_device_.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo(
				global_descriptor_pool,
				1u, &*world_gen_decriptor_set_layout_)).front();

		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].buffer.get(),
			0u,
			chunk_data_buffer_size_);

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
			},
			{});
	}

	// Create initial light fill shader.
	initial_light_fill_shader_= CreateShader(vk_device_, ShaderNames::initial_light_fill_comp);

	// Create initial light fill descriptor set layout.
	{
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
		initial_light_fill_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create initial light fill pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(ChunkPositionUniforms));

		initial_light_fill_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*initial_light_fill_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create initial light fill pipeline.
	initial_light_fill_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*initial_light_fill_shader_,
				"main"),
			*initial_light_fill_pipeline_layout_)));

	// Create and update initial light fill descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		initial_light_fill_descriptor_sets_[i]= vk_device_.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo(
				global_descriptor_pool,
				1u, &*initial_light_fill_decriptor_set_layout_)).front();

		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].buffer.get(),
			0u,
			chunk_data_buffer_size_);

		const vk::DescriptorBufferInfo descriptor_light_data_buffer_info(
			light_buffers_[i].buffer.get(),
			0u,
			light_buffer_size_);

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

	// Create world blocks update shader.
	world_blocks_update_shader_= CreateShader(vk_device_, ShaderNames::world_blocks_update_comp);

	// Create world blocks update descriptor set layout.
	{
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
		};
		world_blocks_update_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create world blocks update pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(ChunkPositionUniforms));

		world_blocks_update_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*world_blocks_update_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create world blocks update pipeline.
	world_blocks_update_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*world_blocks_update_shader_,
				"main"),
			*world_blocks_update_pipeline_layout_)));

	// Create and update world blocks update descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		world_blocks_update_descriptor_sets_[i]= vk_device_.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo(
				global_descriptor_pool,
				1u, &*world_blocks_update_decriptor_set_layout_)).front();

		const vk::DescriptorBufferInfo descriptor_chunk_data_input_buffer_info(
			chunk_data_buffers_[i].buffer.get(),
			0u,
			chunk_data_buffer_size_);

		const vk::DescriptorBufferInfo descriptor_chunk_data_output_buffer_info(
			chunk_data_buffers_[i ^ 1].buffer.get(),
			0u,
			chunk_data_buffer_size_);

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
			},
			{});
	}

	// Create light update shader.
	light_update_shader_= CreateShader(vk_device_, ShaderNames::light_update_comp);

	// Create light update descriptor set layout.
	{
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
		light_update_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create light update pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(ChunkPositionUniforms));

		light_update_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*light_update_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create light update pipeline.
	light_update_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*light_update_shader_,
				"main"),
			*light_update_pipeline_layout_)));

	// Create and update light update descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		light_update_descriptor_sets_[i]= vk_device_.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo(
				global_descriptor_pool,
				1u, &*light_update_decriptor_set_layout_)).front();

		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].buffer.get(),
			0u,
			chunk_data_buffer_size_);

		const vk::DescriptorBufferInfo descriptor_input_light_data_buffer_info(
			light_buffers_[i].buffer.get(),
			0u,
			light_buffer_size_);

		const vk::DescriptorBufferInfo descriptor_output_light_data_buffer_info(
			light_buffers_[i ^ 1].buffer.get(),
			0u,
			light_buffer_size_);

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

	// Create player world window build shader.
	player_world_window_build_shader_= CreateShader(vk_device_, ShaderNames::player_world_window_build_comp);

	// Create player world window build descriptor set layout.
	{
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
		player_world_window_build_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create player world window build pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(PlayerWorldWindowBuildUniforms));

		player_world_window_build_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*player_world_window_build_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create player world window build pipeline.
	player_world_window_build_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*player_world_window_build_shader_,
				"main"),
			*player_world_window_build_pipeline_layout_)));

	// Create and update player world window build descriptor set.
	for(uint32_t i= 0; i < 2; ++i)
	{
		player_world_window_build_descriptor_sets_[i]= vk_device_.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo(
				global_descriptor_pool,
				1u, &*player_world_window_build_decriptor_set_layout_)).front();

		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].buffer.get(),
			0u,
			chunk_data_buffer_size_);

		const vk::DescriptorBufferInfo player_world_window_buffer_info(
			player_world_window_buffer_.buffer.get(),
			0u,
			sizeof(PlayerWorldWindow));

		const vk::DescriptorBufferInfo player_state_buffer_info(
			player_state_buffer_.buffer.get(),
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

	// Create player update shader.
	player_update_shader_= CreateShader(vk_device_, ShaderNames::player_update_comp);

	// Create player update descriptor set layout.
	{
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
		player_update_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create player update pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(PlayerUpdateUniforms));

		player_update_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*player_update_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create player update pipeline.
	player_update_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*player_update_shader_,
				"main"),
			*player_update_pipeline_layout_)));

	// Create player update descriptor set.
	player_update_descriptor_set_= vk_device_.allocateDescriptorSets(
		vk::DescriptorSetAllocateInfo(
			global_descriptor_pool,
			1u, &*player_update_decriptor_set_layout_)).front();

	// Update player update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_player_state_buffer_info(
			player_state_buffer_.buffer.get(),
			0u,
			sizeof(PlayerState));

		const vk::DescriptorBufferInfo world_blocks_external_update_queue_buffer_info(
			world_blocks_external_update_queue_buffer_.buffer.get(),
			0u,
			sizeof(WorldBlocksExternalUpdateQueue));

		const vk::DescriptorBufferInfo player_world_window_buffer_info(
			player_world_window_buffer_.buffer.get(),
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

	// Create world blocks external update queue flush shader.
	world_blocks_external_update_queue_flush_shader_= CreateShader(vk_device_, ShaderNames::world_blocks_external_queue_flush_comp);

	// Create world blocks external update queue flush descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				WorldBlocksExternalUpdateQueueFlushBindigns::chunk_data_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
			{
				WorldBlocksExternalUpdateQueueFlushBindigns::world_blocks_external_update_queue_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
		};
		world_blocks_external_update_queue_flush_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create world blocks external update queue flush pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(WorldBlocksExternalUpdateQueueFlushUniforms));

		world_blocks_external_update_queue_flush_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*world_blocks_external_update_queue_flush_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create world blocks external update queue flush pipeline.
	world_blocks_external_update_queue_flush_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*world_blocks_external_update_queue_flush_shader_,
				"main"),
			*world_blocks_external_update_queue_flush_pipeline_layout_)));

	// Create and update world blocks external update queue flush descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		world_blocks_external_update_queue_flush_descriptor_sets_[i]= vk_device_.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo(
				global_descriptor_pool,
				1u, &*world_blocks_external_update_queue_flush_decriptor_set_layout_)).front();

		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffers_[i].buffer.get(),
			0u,
			chunk_data_buffer_size_);

		const vk::DescriptorBufferInfo world_blocks_external_update_queue_buffer_info(
			world_blocks_external_update_queue_buffer_.buffer.get(),
			0u,
			sizeof(WorldBlocksExternalUpdateQueue));

		vk_device_.updateDescriptorSets(
			{
				{
					world_blocks_external_update_queue_flush_descriptor_sets_[i],
					WorldBlocksExternalUpdateQueueFlushBindigns::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					world_blocks_external_update_queue_flush_descriptor_sets_[i],
					WorldBlocksExternalUpdateQueueFlushBindigns::world_blocks_external_update_queue_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&world_blocks_external_update_queue_buffer_info,
					nullptr
				},
			},
			{});
	}
}

WorldProcessor::~WorldProcessor()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldProcessor::Update(
	const vk::CommandBuffer command_buffer,
	const float time_delta_s,
	const KeyboardState keyboard_state,
	const MouseState mouse_state,
	const float aspect)
{
	InitialFillBuffers(command_buffer);
	GenerateWorld(command_buffer);

	const float c_update_frequency= 4.0f; // TODO - tune this.

	// Do not allow updating slightly less than whole world in a frame.
	const float tick_delta_clamped= std::min(time_delta_s * c_update_frequency, 0.75f);

	const float prev_tick_fractional= current_tick_fractional_;
	const float cur_tick_fractional= current_tick_fractional_ + tick_delta_clamped;

	// Continue updates of blocks and lighting.
	if(current_tick_ != 0)
	{
		const float prev_offset_within_tick= prev_tick_fractional - float(current_tick_);
		HEX_ASSERT(prev_offset_within_tick <= 1.0f);
		const float cur_offset_within_tick= std::min(1.0f, cur_tick_fractional - float(current_tick_));
		BuildCurrentFrameChunksToUpdateList(prev_offset_within_tick, cur_offset_within_tick);

		UpdateWorldBlocks(command_buffer);
		UpdateLight(command_buffer);
		// No need to synchronize world blocks and lighting update here.
		// Add a barier only at the beginning of next tick.
	}

	// Switch to the next tick (if necessary).
	if(current_tick_ == 0 || uint32_t(prev_tick_fractional) < uint32_t(cur_tick_fractional))
	{
		CreateWorldBlocksAndLightUpdateBarrier(command_buffer); // Wait until all block and light updates are finished.

		FlushWorldBlocksExternalUpdateQueue(command_buffer);

		++current_tick_;
		current_tick_fractional_= float(current_tick_);

		BuildPlayerWorldWindow(command_buffer);
	}
	else
		current_tick_fractional_= cur_tick_fractional;

	// Run player update independent on world update - every frame.
	// This is needed in order to make player movement and rotation smooth.
	UpdatePlayer(command_buffer, time_delta_s, keyboard_state, mouse_state, aspect);
}

vk::Buffer WorldProcessor::GetChunkDataBuffer(const uint32_t index) const
{
	HEX_ASSERT(index < 2);
	return chunk_data_buffers_[index].buffer.get();
}

uint32_t WorldProcessor::GetChunkDataBufferSize() const
{
	return chunk_data_buffer_size_;
}

vk::Buffer WorldProcessor::GetLightDataBuffer(const uint32_t index) const
{
	HEX_ASSERT(index < 2);
	return light_buffers_[index].buffer.get();
}

uint32_t WorldProcessor::GetLightDataBufferSize() const
{
	return light_buffer_size_;
}

vk::Buffer WorldProcessor::GetPlayerStateBuffer() const
{
	return player_state_buffer_.buffer.get();
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

void WorldProcessor::InitialFillBuffers(const vk::CommandBuffer command_buffer)
{
	if(initial_buffers_filled_)
		return;
	initial_buffers_filled_= true;

	// Zero chunk data buffers in order to prevent bugs with uninitialized memory.
	for(uint32_t i= 0; i < 2; ++i)
		command_buffer.fillBuffer(*chunk_data_buffers_[i].buffer, 0, chunk_data_buffer_size_, 0);

	for(uint32_t i= 0; i < 2; ++i)
		command_buffer.fillBuffer(*light_buffers_[i].buffer, 0, light_buffer_size_, 0);

	// Set initial player state.
	{
		PlayerState player_state;
		player_state.pos[0]= 0.0f;
		player_state.pos[1]= 0.0f;
		player_state.pos[2]= 40.0f;
		player_state.build_block_type= BlockType::Stone;

		command_buffer.updateBuffer(
			*player_state_buffer_.buffer,
			0,
			sizeof(PlayerState),
			static_cast<const void*>(&player_state));
	}

	command_buffer.fillBuffer(*world_blocks_external_update_queue_buffer_.buffer, 0, sizeof(WorldBlocksExternalUpdateQueue), 0);

	command_buffer.fillBuffer(*player_world_window_buffer_.buffer, 0, sizeof(PlayerWorldWindow), 0);

	const vk::BufferMemoryBarrier barriers[]
	{
		{
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_data_buffers_[0].buffer,
			0,
			VK_WHOLE_SIZE,
		},
		{
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_data_buffers_[1].buffer,
			0,
			VK_WHOLE_SIZE,
		},
		{
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*light_buffers_[0].buffer,
			0,
			VK_WHOLE_SIZE,
		},
		{
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*light_buffers_[1].buffer,
			0,
			VK_WHOLE_SIZE,
		},
		{
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*player_state_buffer_.buffer,
			0,
			VK_WHOLE_SIZE,
		},
		{
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*world_blocks_external_update_queue_buffer_.buffer,
			0,
			VK_WHOLE_SIZE,
		},
		{
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*player_world_window_buffer_.buffer,
			0,
			VK_WHOLE_SIZE,
		},
	};

	command_buffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eComputeShader,
		vk::DependencyFlags(),
		0, nullptr,
		uint32_t(std::size(barriers)), barriers,
		0, nullptr);
}

void WorldProcessor::GenerateWorld(const vk::CommandBuffer command_buffer)
{
	if(world_generated_)
		return;
	world_generated_= true;

	const uint32_t dst_buffer_index= GetDstBufferIndex();

	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *world_gen_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*world_gen_pipeline_layout_,
		0u,
		1u, &world_gen_descriptor_sets_[dst_buffer_index],
		0u, nullptr);

	for(uint32_t x= 0; x < world_size_[0]; ++x)
	for(uint32_t y= 0; y < world_size_[1]; ++y)
	{
		ChunkPositionUniforms chunk_position_uniforms;
		chunk_position_uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
		chunk_position_uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
		chunk_position_uniforms.chunk_position[0]= int32_t(x);
		chunk_position_uniforms.chunk_position[1]= int32_t(y);
		chunk_position_uniforms.chunk_global_position[0]= world_offset_[0] + int32_t(x);
		chunk_position_uniforms.chunk_global_position[1]= world_offset_[1] + int32_t(y);

		command_buffer.pushConstants(
			*world_gen_pipeline_layout_,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(ChunkPositionUniforms), static_cast<const void*>(&chunk_position_uniforms));

		// This constant should match workgroup size in shader!
		constexpr uint32_t c_workgroup_size[]{8, 8, 1};
		static_assert(c_chunk_width % c_workgroup_size[0] == 0, "Wrong workgroup size!");
		static_assert(c_chunk_width % c_workgroup_size[1] == 0, "Wrong workgroup size!");
		static_assert(c_chunk_height % c_workgroup_size[2] == 0, "Wrong workgroup size!");

		// Dispatch only 2D group - perform generation for columns.
		command_buffer.dispatch(
			c_chunk_width / c_workgroup_size[0],
			c_chunk_width / c_workgroup_size[1],
			1 / c_workgroup_size[2]);
	}

	// Create barrier between world generation and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_data_buffers_[dst_buffer_index].buffer,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}

	// Perform initial light fill.

	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *initial_light_fill_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*initial_light_fill_pipeline_layout_,
		0u,
		1u, &initial_light_fill_descriptor_sets_[dst_buffer_index],
		0u, nullptr);

	for(uint32_t x= 0; x < world_size_[0]; ++x)
	for(uint32_t y= 0; y < world_size_[1]; ++y)
	{
		ChunkPositionUniforms chunk_position_uniforms;
		chunk_position_uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
		chunk_position_uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
		chunk_position_uniforms.chunk_position[0]= int32_t(x);
		chunk_position_uniforms.chunk_position[1]= int32_t(y);
		chunk_position_uniforms.chunk_global_position[0]= world_offset_[0] + int32_t(x);
		chunk_position_uniforms.chunk_global_position[1]= world_offset_[1] + int32_t(y);

		command_buffer.pushConstants(
			*initial_light_fill_pipeline_layout_,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(ChunkPositionUniforms), static_cast<const void*>(&chunk_position_uniforms));

		// This constant should match workgroup size in shader!
		constexpr uint32_t c_workgroup_size[]{8, 8, 1};
		static_assert(c_chunk_width % c_workgroup_size[0] == 0, "Wrong workgroup size!");
		static_assert(c_chunk_width % c_workgroup_size[1] == 0, "Wrong workgroup size!");
		static_assert(c_chunk_height % c_workgroup_size[2] == 0, "Wrong workgroup size!");

		// Dispatch only 2D group - perform light fill for columns.
		command_buffer.dispatch(
			c_chunk_width / c_workgroup_size[0],
			c_chunk_width / c_workgroup_size[1],
			1 / c_workgroup_size[2]);
	}

	// Create barrier between light buffer fill and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*light_buffers_[dst_buffer_index].buffer,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
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

void WorldProcessor::UpdateWorldBlocks(const vk::CommandBuffer command_buffer)
{
	const uint32_t src_buffer_index= GetSrcBufferIndex();

	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *world_blocks_update_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*world_blocks_update_pipeline_layout_,
		0u,
		1u, &world_blocks_update_descriptor_sets_[src_buffer_index],
		0u, nullptr);

	for(const auto& chunk_to_update : current_frame_chunks_to_update_list_)
	{
		ChunkPositionUniforms chunk_position_uniforms;
		chunk_position_uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
		chunk_position_uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
		chunk_position_uniforms.chunk_position[0]= int32_t(chunk_to_update[0]);
		chunk_position_uniforms.chunk_position[1]= int32_t(chunk_to_update[1]);
		chunk_position_uniforms.chunk_global_position[0]= world_offset_[0] + int32_t(chunk_to_update[0]);
		chunk_position_uniforms.chunk_global_position[1]= world_offset_[1] + int32_t(chunk_to_update[1]);

		command_buffer.pushConstants(
			*world_blocks_update_pipeline_layout_,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(ChunkPositionUniforms), static_cast<const void*>(&chunk_position_uniforms));

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
}

void WorldProcessor::UpdateLight(const vk::CommandBuffer command_buffer)
{
	const uint32_t src_buffer_index= GetSrcBufferIndex();

	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *light_update_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*light_update_pipeline_layout_,
		0u,
		1u, &light_update_descriptor_sets_[src_buffer_index],
		0u, nullptr);

	for(const auto& chunk_to_update : current_frame_chunks_to_update_list_)
	{
		ChunkPositionUniforms chunk_position_uniforms;
		chunk_position_uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
		chunk_position_uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
		chunk_position_uniforms.chunk_position[0]= int32_t(chunk_to_update[0]);
		chunk_position_uniforms.chunk_position[1]= int32_t(chunk_to_update[1]);
		chunk_position_uniforms.chunk_global_position[0]= world_offset_[0] + int32_t(chunk_to_update[0]);
		chunk_position_uniforms.chunk_global_position[1]= world_offset_[1] + int32_t(chunk_to_update[1]);

		command_buffer.pushConstants(
			*light_update_pipeline_layout_,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(ChunkPositionUniforms), static_cast<const void*>(&chunk_position_uniforms));

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
}

void WorldProcessor::CreateWorldBlocksAndLightUpdateBarrier(const vk::CommandBuffer command_buffer)
{
	// Wait until blocks and light updates are finished.
	// TODO - check if this is correct.
	const uint32_t dst_buffer_index= GetDstBufferIndex();

	const vk::BufferMemoryBarrier barriers[]
	{
		{
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_data_buffers_[dst_buffer_index].buffer,
			0,
			VK_WHOLE_SIZE
		},
		{
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*light_buffers_[dst_buffer_index].buffer,
			0,
			VK_WHOLE_SIZE
		},
	};

	command_buffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eComputeShader,
		vk::PipelineStageFlagBits::eComputeShader,
		vk::DependencyFlags(),
		0, nullptr,
		uint32_t(std::size(barriers)), barriers,
		0, nullptr);
}

void WorldProcessor::BuildPlayerWorldWindow(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *player_world_window_build_pipeline_);

	// Build player world window based on src world state.
	const auto descriptor_set= player_world_window_build_descriptor_sets_[GetSrcBufferIndex()];

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*player_world_window_build_pipeline_layout_,
		0u,
		1u, &descriptor_set,
		0u, nullptr);

	PlayerWorldWindowBuildUniforms uniforms;
	uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
	uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
	uniforms.world_offset_chunks[0]= world_offset_[0];
	uniforms.world_offset_chunks[1]= world_offset_[1];

	command_buffer.pushConstants(
		*player_world_window_build_pipeline_layout_,
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

	// Create barrier between player world window buffer building and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*player_world_window_buffer_.buffer,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

void WorldProcessor::UpdatePlayer(
	const vk::CommandBuffer command_buffer,
	const float time_delta_s,
	const KeyboardState keyboard_state,
	const MouseState mouse_state,
	const float aspect)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *player_update_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*player_update_pipeline_layout_,
		0u,
		1u, &player_update_descriptor_set_,
		0u, nullptr);

	PlayerUpdateUniforms player_update_uniforms;
	player_update_uniforms.aspect= aspect;
	player_update_uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
	player_update_uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
	player_update_uniforms.time_delta_s= time_delta_s;
	player_update_uniforms.keyboard_state= keyboard_state;
	player_update_uniforms.mouse_state= mouse_state;

	command_buffer.pushConstants(
		*player_update_pipeline_layout_,
		vk::ShaderStageFlagBits::eCompute,
		0,
		sizeof(PlayerUpdateUniforms), static_cast<const void*>(&player_update_uniforms));

	// Player update is single-threaded.
	command_buffer.dispatch(1, 1, 1);

	// Create barrier between player update and later player state usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eTransferRead,
			queue_family_index_, queue_family_index_,
			*player_state_buffer_.buffer,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
	// Create barrier after usage of world blocks external update queue.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*world_blocks_external_update_queue_buffer_.buffer,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
	// Create barrier between player world window buffer update and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*player_world_window_buffer_.buffer,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

void WorldProcessor::FlushWorldBlocksExternalUpdateQueue(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *world_blocks_external_update_queue_flush_pipeline_);

	// Flush the queue into the destination world buffer.
	const uint32_t dst_buffer_index= GetDstBufferIndex();
	const auto descriptor_set= world_blocks_external_update_queue_flush_descriptor_sets_[dst_buffer_index];

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*world_blocks_external_update_queue_flush_pipeline_layout_,
		0u,
		1u, &descriptor_set,
		0u, nullptr);

	WorldBlocksExternalUpdateQueueFlushUniforms uniforms;
	uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
	uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
	uniforms.world_offset_chunks[0]= world_offset_[0];
	uniforms.world_offset_chunks[1]= world_offset_[1];

	command_buffer.pushConstants(
		*player_update_pipeline_layout_,
		vk::ShaderStageFlagBits::eCompute,
		0,
		sizeof(WorldBlocksExternalUpdateQueueFlushUniforms), static_cast<const void*>(&uniforms));

	// Queue flushing is single-threaded.
	command_buffer.dispatch(1, 1, 1);

	// Create barrier between world changes in queue flush and world later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_data_buffers_[dst_buffer_index].buffer,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
	// Create barrier after usage of world blocks external update queue.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*world_blocks_external_update_queue_buffer_.buffer,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
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
