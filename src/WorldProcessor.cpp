#include "WorldProcessor.hpp"
#include "Constants.hpp"
#include "ShaderList.hpp"

namespace HexGPU
{

namespace
{

namespace WorldGenShaderBindings
{

uint32_t chunk_data_buffer= 0;

}

// Size limit of this struct is 128 bytes.
// 128 bytes is guaranted maximum size of push constants uniform block.
struct ChunkPositionUniforms
{
	int32_t chunk_position[2];
};

struct PlayerUpdateUniforms
{
	m_Vec3 player_pos;
	float reserved0= 0.0f;
	m_Vec3 player_dir;
	float reserved1= 0.0f;
	bool build_triggered= false;
	bool destroy_triggered= false;
	uint8_t reserved2[2];
};

} // namespace

WorldProcessor::WorldProcessor(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
{
	// Create chunk data buffer.
	{
		chunk_data_buffer_size_= c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1];

		chunk_data_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					chunk_data_buffer_size_,
					vk::BufferUsageFlagBits::eStorageBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*chunk_data_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags() &&
				// TODO - avoid making host visible.
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				memory_allocate_info.memoryTypeIndex= i;
		}

		chunk_data_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*chunk_data_buffer_, *chunk_data_buffer_memory_, 0u);

		// Fill the buffer with zeros to prevent later warnings.
		void* data_gpu_side= nullptr;
		vk_device_.mapMemory(*chunk_data_buffer_memory_, 0u, memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &data_gpu_side);
		std::memset(data_gpu_side, 0, chunk_data_buffer_size_);
		vk_device_.unmapMemory(*chunk_data_buffer_memory_);
	}

	// Create player state buffer.
	{
		player_state_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					sizeof(PlayerState),
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*player_state_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags() &&
				// TODO - avoid making host visible.
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				memory_allocate_info.memoryTypeIndex= i;
		}

		player_state_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*player_state_buffer_, *player_state_buffer_memory_, 0u);

		// Fill the buffer with zeros to prevent later warnings.
		void* data_gpu_side= nullptr;
		vk_device_.mapMemory(*player_state_buffer_memory_, 0u, memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &data_gpu_side);
		std::memset(data_gpu_side, 0, sizeof(PlayerState));
		vk_device_.unmapMemory(*player_state_buffer_memory_);
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
	world_gen_pipeline_= vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*world_gen_shader_,
				"main"),
			*world_gen_pipeline_layout_));

	// Create world generation descriptor set pool.
	{
		const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1u /*num descriptors*/);
		world_gen_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					1u, // max sets.
					1u, &descriptor_pool_size));
	}

	// Create world generation descriptor set.
	world_gen_descriptor_set_=
		std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*world_gen_descriptor_pool_,
					1u, &*world_gen_decriptor_set_layout_)).front());

	// Update world generator descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffer_.get(),
			0u,
			chunk_data_buffer_size_);

		vk_device_.updateDescriptorSets(
			{
				{
					*world_gen_descriptor_set_,
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

	// Create player update shader.
	player_update_shader_= CreateShader(vk_device_, ShaderNames::player_update_comp);

	// Create player update descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				0u,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
			{
				1u,
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
	player_update_pipeline_= vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*player_update_shader_,
				"main"),
			*player_update_pipeline_layout_));

	// Create player update descriptor set pool.
	{
		const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 2u /*num descriptors*/);
		player_update_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					1u, // max sets.
					1u, &descriptor_pool_size));
	}

	// Create player update descriptor set.
	player_update_descriptor_set_=
		std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*player_update_descriptor_pool_,
					1u, &*player_update_decriptor_set_layout_)).front());

	// Update player update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			chunk_data_buffer_.get(),
			0u,
			chunk_data_buffer_size_);

		const vk::DescriptorBufferInfo descriptor_player_state_buffer_info(
			player_state_buffer_.get(),
			0u,
			sizeof(PlayerState));

		vk_device_.updateDescriptorSets(
			{
				{
					*player_update_descriptor_set_,
					0u,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					*player_update_descriptor_set_,
					0u,
					1u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_player_state_buffer_info,
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
	const m_Vec3& player_pos,
	const m_Vec3& player_dir,
	const bool build_triggered,
	const bool destroy_triggered)
{
	if(!world_generated_)
	{
		world_generated_= true;

		// Run world generation.

		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *world_gen_pipeline_);

		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			*world_gen_pipeline_layout_,
			0u,
			1u, &*world_gen_descriptor_set_,
			0u, nullptr);

		for(uint32_t x= 0; x < c_chunk_matrix_size[0]; ++x)
		for(uint32_t y= 0; y < c_chunk_matrix_size[1]; ++y)
		{
			ChunkPositionUniforms chunk_position_uniforms;
			chunk_position_uniforms.chunk_position[0]= int32_t(x);
			chunk_position_uniforms.chunk_position[1]= int32_t(y);

			command_buffer.pushConstants(
				*world_gen_pipeline_layout_,
				vk::ShaderStageFlagBits::eCompute,
				0,
				sizeof(ChunkPositionUniforms), static_cast<const void*>(&chunk_position_uniforms));

			// Dispatch only 2D group - perform generation for columns.
			command_buffer.dispatch(c_chunk_width, c_chunk_width , 1);
		}

		// Create barrier between world generation and its later usage.
		// TODO - check this is correct.
		{
			vk::BufferMemoryBarrier barrier;
			barrier.srcAccessMask= vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
			barrier.dstAccessMask= vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
			barrier.size= VK_WHOLE_SIZE;
			barrier.buffer= *chunk_data_buffer_;
			barrier.srcQueueFamilyIndex= queue_family_index_;
			barrier.dstQueueFamilyIndex= queue_family_index_;

			command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eComputeShader,
				vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(),
				0, nullptr,
				1, &barrier,
				0, nullptr);
		}
	}

	// Update player.
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *player_update_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*player_update_pipeline_layout_,
		0u,
		1u, &*player_update_descriptor_set_,
		0u, nullptr);

	PlayerUpdateUniforms player_update_uniforms;
	player_update_uniforms.player_pos= player_pos;
	player_update_uniforms.player_dir= player_dir;
	player_update_uniforms.build_triggered= build_triggered;
	player_update_uniforms.destroy_triggered= destroy_triggered;

	command_buffer.pushConstants(
		*player_update_pipeline_layout_,
		vk::ShaderStageFlagBits::eCompute,
		0,
		sizeof(PlayerUpdateUniforms), static_cast<const void*>(&player_update_uniforms));

	// Player update is single-threaded.
	command_buffer.dispatch(1, 1, 1);

	// Create barrier between player update and world later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_data_buffer_,
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
	// Create barrier between player update and later player state usage.
	// TODO - check this is correct.
	{
		vk::BufferMemoryBarrier barrier;
		barrier.srcAccessMask= vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		barrier.dstAccessMask= vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		barrier.size= VK_WHOLE_SIZE;
		barrier.buffer= *player_state_buffer_;
		barrier.srcQueueFamilyIndex= queue_family_index_;
		barrier.dstQueueFamilyIndex= queue_family_index_;

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

vk::Buffer WorldProcessor::GetChunkDataBuffer() const
{
	return chunk_data_buffer_.get();
}

uint32_t WorldProcessor::GetChunkDataBufferSize() const
{
	return chunk_data_buffer_size_;
}

vk::Buffer WorldProcessor::GetPlayerStateBuffer() const
{
	return player_state_buffer_.get();
}

} // namespace HexGPU
