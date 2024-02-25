#include "WorldGeometryGenerator.hpp"
#include "Assert.hpp"
#include "Constants.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

namespace GeometrySizeCalculatePrepareBindings
{

// This should match bindings in the shader itself!
const uint32_t chunk_draw_info_buffer= 0;

}

namespace GeometrySizeCalculateBindings
{

// This should match bindings in the shader itself!
const uint32_t chunk_data_buffer= 0;
const uint32_t chunk_draw_info_buffer= 1;

}

namespace GeometryAllocateBindings
{

// This should match bindings in the shader itself!
const uint32_t chunk_draw_info_buffer= 0;
const uint32_t allocator_data_buffer= GPUAllocator::c_allocator_buffer_binding;

}

namespace GeometryGenShaderBindings
{

// This should match bindings in the shader itself!
const uint32_t vertices_buffer= 0;
const uint32_t chunk_data_buffer= 1;
const uint32_t chunk_light_buffer= 2;
const uint32_t chunk_draw_info_buffer= 3;

}

// Size limit of this struct is 128 bytes.
// 128 bytes is guaranted maximum size of push constants uniform block.
struct ChunkPositionUniforms
{
	int32_t chunk_position[2];
};

// This should match the same constant in GLSL code!
const uint32_t c_allocation_unut_size_quads= 512;

// Assuming that amount of total required vertex memory is proportional to total number of chunks,
// multiplied by some factor.
// Assuming that in worst cases (complex geometry) and taking allocator fragmentation into account
// we need to have enough space for so much vertices.
const uint32_t c_max_average_quads_per_chunk= 6144;

const uint32_t c_total_vertex_buffer_quads= c_chunk_matrix_size[0] * c_chunk_matrix_size[1] * c_max_average_quads_per_chunk;

// Number of allocation units is based on number of quads with rounding upwards.
const uint32_t c_total_vertex_buffer_units=
	(c_total_vertex_buffer_quads + (c_allocation_unut_size_quads - 1)) / c_allocation_unut_size_quads;
} // namespace

WorldGeometryGenerator::WorldGeometryGenerator(WindowVulkan& window_vulkan, WorldProcessor& world_processor)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_processor_(world_processor)
	, vertex_memory_allocator_(window_vulkan, c_total_vertex_buffer_units)
{
	// Create chunk draw info buffer.
	{
		chunk_draw_info_buffer_size_= c_chunk_matrix_size[0] * c_chunk_matrix_size[1] * sizeof(ChunkDrawInfo);

		chunk_draw_info_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					chunk_draw_info_buffer_size_,
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*chunk_draw_info_buffer_);

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

		chunk_draw_info_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*chunk_draw_info_buffer_, *chunk_draw_info_buffer_memory_, 0u);
	}

	// Create vertex buffer.
	vertex_buffer_num_quads_= c_total_vertex_buffer_quads;
	{
		const size_t quads_data_size= vertex_buffer_num_quads_ * sizeof(QuadVertices);

		vertex_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					quads_data_size,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vertex_buffer_);

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

		vertex_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*vertex_buffer_, *vertex_buffer_memory_, 0u);
	}

	// Create shaders.
	geometry_size_calculate_prepare_shader_= CreateShader(vk_device_, ShaderNames::geometry_size_calculate_prepare_comp);

	// Create descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				GeometrySizeCalculatePrepareBindings::chunk_draw_info_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
		};
		geometry_size_calculate_prepare_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create pipeline layout.
	geometry_size_calculate_prepare_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*geometry_size_calculate_prepare_decriptor_set_layout_,
			0u, nullptr));

	// Create pipeline.
	geometry_size_calculate_prepare_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*geometry_size_calculate_prepare_shader_,
				"main"),
			*geometry_size_calculate_prepare_pipeline_layout_)));

	// Create descriptor set pool.
	{
		const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1u /*num descriptors*/);
		geometry_size_calculate_prepare_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					1u, // max sets.
					1u, &descriptor_pool_size));
	}

	// Create descriptor set.
	geometry_size_calculate_prepare_descriptor_set_=
		std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*geometry_size_calculate_prepare_descriptor_pool_,
					1u, &*geometry_size_calculate_prepare_decriptor_set_layout_)).front());

	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			*chunk_draw_info_buffer_,
			0u,
			chunk_draw_info_buffer_size_);

		vk_device_.updateDescriptorSets(
			{
				{
					*geometry_size_calculate_prepare_descriptor_set_,
					GeometrySizeCalculatePrepareBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Create shaders.
	geometry_size_calculate_shader_= CreateShader(vk_device_, ShaderNames::geometry_size_calculate_comp);

	// Create descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				GeometrySizeCalculateBindings::chunk_data_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
			{
				GeometrySizeCalculateBindings::chunk_draw_info_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
		};
		geometry_size_calculate_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(ChunkPositionUniforms));

		geometry_size_calculate_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*geometry_size_calculate_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create pipeline.
	geometry_size_calculate_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*geometry_size_calculate_shader_,
				"main"),
			*geometry_size_calculate_pipeline_layout_)));

	// Create descriptor set pool.
	{
		const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1u /*num descriptors*/);
		geometry_size_calculate_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					1u, // max sets.
					1u, &descriptor_pool_size));
	}

	// Create descriptor set.
	geometry_size_calculate_descriptor_set_=
		std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*geometry_size_calculate_descriptor_pool_,
					1u, &*geometry_size_calculate_decriptor_set_layout_)).front());

	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			world_processor_.GetChunkDataBuffer(),
			0u,
			world_processor_.GetChunkDataBufferSize());

		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			*chunk_draw_info_buffer_,
			0u,
			chunk_draw_info_buffer_size_);

		vk_device_.updateDescriptorSets(
			{
				{
					*geometry_size_calculate_descriptor_set_,
					GeometrySizeCalculateBindings::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					*geometry_size_calculate_descriptor_set_,
					GeometrySizeCalculateBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Create shaders.
	geometry_allocate_shader_= CreateShader(vk_device_, ShaderNames::geometry_allocate_comp);

	// Create descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				GeometryAllocateBindings::chunk_draw_info_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
			{
				GeometryAllocateBindings::allocator_data_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
		};
		geometry_allocate_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create pipeline layout.
	geometry_allocate_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*geometry_allocate_decriptor_set_layout_,
			0u, nullptr));

	// Create pipeline.
	geometry_allocate_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*geometry_allocate_shader_,
				"main"),
			*geometry_allocate_pipeline_layout_)));

	// Create descriptor set pool.
	{
		const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1u /*num descriptors*/);
		geometry_allocate_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					1u, // max sets.
					1u, &descriptor_pool_size));
	}

	// Create descriptor set.
	geometry_allocate_descriptor_set_=
		std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*geometry_allocate_descriptor_pool_,
					1u, &*geometry_allocate_decriptor_set_layout_)).front());

	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			*chunk_draw_info_buffer_,
			0u,
			chunk_draw_info_buffer_size_);

		const vk::DescriptorBufferInfo descriptor_allocator_data_buffer_info(
			vertex_memory_allocator_.GetAllocatorDataBuffer(),
			0u,
			vertex_memory_allocator_.GetAllocatorDataBufferSize());

		vk_device_.updateDescriptorSets(
			{
				{
					*geometry_allocate_descriptor_set_,
					GeometryAllocateBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
				{
					*geometry_allocate_descriptor_set_,
					GeometryAllocateBindings::allocator_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_allocator_data_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Create shaders.
	geometry_gen_shader_= CreateShader(vk_device_, ShaderNames::geometry_gen_comp);

	// Create descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				GeometryGenShaderBindings::vertices_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
			{
				GeometryGenShaderBindings::chunk_data_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
			{
				GeometryGenShaderBindings::chunk_light_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
			{
				GeometryGenShaderBindings::chunk_draw_info_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
		};
		geometry_gen_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(ChunkPositionUniforms));

		geometry_gen_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*geometry_gen_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create pipeline.
	geometry_gen_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*geometry_gen_shader_,
				"main"),
			*geometry_gen_pipeline_layout_)));

	// Create descriptor set pool.
	{
		const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 4u /*num descriptors*/);
		geometry_gen_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					1u, // max sets.
					1u, &descriptor_pool_size));
	}

	// Create descriptor set.
	geometry_gen_descriptor_set_=
		std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*geometry_gen_descriptor_pool_,
					1u, &*geometry_gen_decriptor_set_layout_)).front());

	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_vertex_buffer_info(
			*vertex_buffer_,
			0u,
			vertex_buffer_num_quads_ * sizeof(QuadVertices));

		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			world_processor_.GetChunkDataBuffer(),
			0u,
			world_processor_.GetChunkDataBufferSize());

		const vk::DescriptorBufferInfo descriptor_light_buffer_info(
			world_processor_.GetLightDataBuffer(),
			0u,
			world_processor_.GetLightDataBufferSize());

		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			*chunk_draw_info_buffer_,
			0u,
			chunk_draw_info_buffer_size_);

		vk_device_.updateDescriptorSets(
			{
				{
					*geometry_gen_descriptor_set_,
					GeometryGenShaderBindings::vertices_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_vertex_buffer_info,
					nullptr
				},
				{
					*geometry_gen_descriptor_set_,
					GeometryGenShaderBindings::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					*geometry_gen_descriptor_set_,
					GeometryGenShaderBindings::chunk_light_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_light_buffer_info,
					nullptr
				},
				{
					*geometry_gen_descriptor_set_,
					GeometryGenShaderBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
			},
			{});
	}
}

WorldGeometryGenerator::~WorldGeometryGenerator()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldGeometryGenerator::PrepareFrame(const vk::CommandBuffer command_buffer)
{
	vertex_memory_allocator_.EnsureInitialized(command_buffer);
	InitialFillBuffers(command_buffer);
	PrepareGeometrySizeCalculation(command_buffer);
	CalculateGeometrySize(command_buffer);
	AllocateMemoryForGeometry(command_buffer);
	GenGeometry(command_buffer);
}

vk::Buffer WorldGeometryGenerator::GetVertexBuffer() const
{
	return vertex_buffer_.get();
}

vk::Buffer WorldGeometryGenerator::GetChunkDrawInfoBuffer() const
{
	return chunk_draw_info_buffer_.get();
}

uint32_t WorldGeometryGenerator::GetChunkDrawInfoBufferSize() const
{
	return chunk_draw_info_buffer_size_;
}

void WorldGeometryGenerator::InitialFillBuffers(const vk::CommandBuffer command_buffer)
{
	if(buffers_initially_filled_)
		return;
	buffers_initially_filled_= true;

	// Fill initially vertex buffer with zeros.
	// Do this only to supress warnings.

	command_buffer.fillBuffer(*vertex_buffer_, 0, vertex_buffer_num_quads_ * sizeof(QuadVertices), 0);

	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*vertex_buffer_,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}

	// Fill initially chunk draw info buffer with zeros.
	command_buffer.fillBuffer(*chunk_draw_info_buffer_, 0, chunk_draw_info_buffer_size_, 0);

	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_draw_info_buffer_,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

void WorldGeometryGenerator::PrepareGeometrySizeCalculation(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *geometry_size_calculate_prepare_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*geometry_size_calculate_prepare_pipeline_layout_,
		0u,
		1u, &*geometry_size_calculate_prepare_descriptor_set_,
		0u, nullptr);

	// Dispatch a thread for each chunk.
	command_buffer.dispatch(c_chunk_matrix_size[0], c_chunk_matrix_size[1], 1);

	// Create barrier between update chunk draw info buffer and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_draw_info_buffer_,
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

void WorldGeometryGenerator::CalculateGeometrySize(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *geometry_size_calculate_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*geometry_size_calculate_pipeline_layout_,
		0u,
		1u, &*geometry_size_calculate_descriptor_set_,
		0u, nullptr);

	// Execute geometry size calculation for all chunks.
	// TODO - do this no each frame and not for each chunk.
	for(uint32_t x= 0; x < c_chunk_matrix_size[0]; ++x)
	for(uint32_t y= 0; y < c_chunk_matrix_size[1]; ++y)
	{
		ChunkPositionUniforms chunk_position_uniforms;
		chunk_position_uniforms.chunk_position[0]= int32_t(x);
		chunk_position_uniforms.chunk_position[1]= int32_t(y);

		command_buffer.pushConstants(
			*geometry_size_calculate_pipeline_layout_,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(ChunkPositionUniforms), static_cast<const void*>(&chunk_position_uniforms));

		// Dispatch a thread for each block in chunk.
		command_buffer.dispatch(
			c_chunk_width / 4,
			c_chunk_width / 4,
			c_chunk_height / 8);
	}

	// Create barrier between update chunk draw info buffer and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_draw_info_buffer_,
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

void WorldGeometryGenerator::AllocateMemoryForGeometry(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *geometry_allocate_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*geometry_allocate_pipeline_layout_,
		0u,
		1u, &*geometry_allocate_descriptor_set_,
		0u, nullptr);

	// Use single thread for allocation.
	command_buffer.dispatch(1, 1 , 1);

	// Create barrier between update chunk draw info buffer and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_draw_info_buffer_,
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

void WorldGeometryGenerator::GenGeometry(const vk::CommandBuffer command_buffer)
{
	// Update geometry, count number of quads.

	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *geometry_gen_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*geometry_gen_pipeline_layout_,
		0u,
		1u, &*geometry_gen_descriptor_set_,
		0u, nullptr);

	// Execute geometry generation for all chunks.
	// TODO - do this no each frame and not for each chunk.
	for(uint32_t x= 0; x < c_chunk_matrix_size[0]; ++x)
	for(uint32_t y= 0; y < c_chunk_matrix_size[1]; ++y)
	{
		ChunkPositionUniforms chunk_position_uniforms;
		chunk_position_uniforms.chunk_position[0]= int32_t(x);
		chunk_position_uniforms.chunk_position[1]= int32_t(y);

		command_buffer.pushConstants(
			*geometry_gen_pipeline_layout_,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(ChunkPositionUniforms), static_cast<const void*>(&chunk_position_uniforms));

		// Dispatch a thread for each block in chunk.
		command_buffer.dispatch(
			c_chunk_width / 4,
			c_chunk_width / 4,
			c_chunk_height / 8);
	}

	// Create barrier between update vertex buffer and its usage for rendering.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*vertex_buffer_,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eVertexShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}

	// Create barrier between update chunk draw info buffer and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*chunk_draw_info_buffer_,
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

} // namespace HexGPU
