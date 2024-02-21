#include "WorldGeometryGenerator.hpp"
#include "Constants.hpp"
#include "Assert.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

// If this changed, the same struct in GLSL code must be changed too!
struct ChunkDrawInfo
{
	uint32_t num_quads= 0;
};

namespace GeometryGenShaderBindings
{

// This should match bindings in the shader itself!
const uint32_t vertices_buffer= 0;
const uint32_t draw_indirect_buffer= 1;
const uint32_t chunk_data_buffer= 2;
const uint32_t chunk_light_buffer= 3;
const uint32_t chunk_draw_info_buffer= 4;

}

// Size limit of this struct is 128 bytes.
// 128 bytes is guaranted maximum size of push constants uniform block.
struct ChunkPositionUniforms
{
	int32_t chunk_position[2];
};

// For now allocate for each chunk maximum possible vertex count.
// TODO - improve this, use some kind of allocator for vertices.
const uint32_t c_max_quads_per_chunk= 65536 / 4;

} // namespace

WorldGeometryGenerator::WorldGeometryGenerator(WindowVulkan& window_vulkan, WorldProcessor& world_processor)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_processor_(world_processor)
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

	// Create shaders.
	geometry_gen_shader_= CreateShader(vk_device_, ShaderNames::geometry_gen_comp);

	// Create vertex buffer.
	vertex_buffer_num_quads_= c_max_quads_per_chunk * c_chunk_matrix_size[0] * c_chunk_matrix_size[1];
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

	// Create draw indirect buffer.
	{
		const size_t num_commands= c_chunk_matrix_size[0] * c_chunk_matrix_size[1];
		const size_t buffer_size= sizeof(vk::DrawIndexedIndirectCommand) * num_commands;

		draw_indirect_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					buffer_size,
					vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*draw_indirect_buffer_);

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

		draw_indirect_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*draw_indirect_buffer_, *draw_indirect_buffer_memory_, 0u);
	}

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
				GeometryGenShaderBindings::draw_indirect_buffer,
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
		const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 5u /*num descriptors*/);
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

		const vk::DescriptorBufferInfo descriptor_draw_indirect_buffer_info(
			*draw_indirect_buffer_,
			0u,
			sizeof(vk::DrawIndexedIndirectCommand) * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]);

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
					GeometryGenShaderBindings::draw_indirect_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_draw_indirect_buffer_info,
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
	if(!vertex_buffer_initially_filled_)
	{
		// Fill initially vertex buffer with zeros.
		// Do this only to supress warnings.

		vertex_buffer_initially_filled_= true;
		command_buffer.fillBuffer(*vertex_buffer_, 0, vertex_buffer_num_quads_ * sizeof(QuadVertices), 0);

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

	if(!chunk_draw_info_buffer_initially_filled_)
	{
		// Fill initially chunk draw info buffer with zeros.
		chunk_draw_info_buffer_initially_filled_= true;

		command_buffer.fillBuffer(*chunk_draw_info_buffer_, 0, chunk_draw_info_buffer_size_, 0);

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

	// Reset draw commands.
	{
		vk::DrawIndexedIndirectCommand commands[ c_chunk_matrix_size[0] * c_chunk_matrix_size[1] ];
		for(uint32_t x= 0; x < c_chunk_matrix_size[0]; ++x)
		for(uint32_t y= 0; y < c_chunk_matrix_size[1]; ++y)
		{
			const uint32_t chunk_index= x + y * c_chunk_matrix_size[0];
			vk::DrawIndexedIndirectCommand& command= commands[ chunk_index ];
			command.indexCount= 0;
			command.instanceCount= 1;
			command.firstIndex= 0;
			command.vertexOffset= chunk_index * c_max_quads_per_chunk * 4;
			command.firstInstance= 0;
		}

		command_buffer.updateBuffer(
			*draw_indirect_buffer_,
			0,
			sizeof(vk::DrawIndexedIndirectCommand) * c_chunk_matrix_size[0] * c_chunk_matrix_size[1],
			static_cast<const void*>(commands));
	}

	// Create barrier between draw indirect buffer update and its usage in shader.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*draw_indirect_buffer_,
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

		command_buffer.dispatch(c_chunk_width, c_chunk_width , c_chunk_height - 1 /*skip last blocks layer*/);
	}
}

vk::Buffer WorldGeometryGenerator::GetVertexBuffer() const
{
	return vertex_buffer_.get();
}

vk::Buffer WorldGeometryGenerator::GetDrawIndirectBuffer() const
{
	return draw_indirect_buffer_.get();
}

} // namespace HexGPU
