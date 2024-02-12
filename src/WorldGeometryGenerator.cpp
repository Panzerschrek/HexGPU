#include "WorldGeometryGenerator.hpp"
#include "Constants.hpp"
#include "Assert.hpp"
#include "ShaderList.hpp"

namespace HexGPU
{

namespace
{

void FillChunkData(uint8_t* data)
{
	for(uint32_t x= 0; x < c_chunk_width; ++x)
	for(uint32_t y= 0; y < c_chunk_width; ++y)
	for(uint32_t z= 0; z < c_chunk_height; ++z)
	{
		data[ChunkBlockAddress(x, y, z)]= z > c_chunk_height / 2 ? 0 : 1;
	}
}

namespace GeometryGenShaderBindings
{

// This should match bindings in the shader itself!
const uint32_t vertices_buffer= 0;
const uint32_t draw_indirect_buffer= 1;
const uint32_t chunk_data_buffer= 2;

}

} // namespace

WorldGeometryGenerator::WorldGeometryGenerator(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
{
	// Create shaders.
	geometry_gen_shader_= CreateShader(vk_device_, ShaderNames::geometry_gen_comp);

	// Create chung data buffer.
	{
		const size_t data_size= c_chunk_volume;

		vk_chunk_data_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					data_size,
					vk::BufferUsageFlagBits::eStorageBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_chunk_data_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_chunk_data_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_chunk_data_buffer_, *vk_chunk_data_buffer_memory_, 0u);

		// Fil lthe buffer with initial values.
		void* data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_chunk_data_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &data_gpu_size);
		FillChunkData(reinterpret_cast<uint8_t*>(data_gpu_size));
		vk_device_.unmapMemory(*vk_chunk_data_buffer_memory_);
	}

	// Create vertex buffer.
	// TODO - make it bigger.
	vertex_buffer_num_quads_= 65536 / 4 - 1;
	{
		const size_t quads_data_size= vertex_buffer_num_quads_ * sizeof(QuadVertices);

		vk_vertex_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					quads_data_size,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_vertex_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_vertex_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_vertex_buffer_, *vk_vertex_buffer_memory_, 0u);

		// Fill the buffer with zeros (to prevent warnings).
		// Anyway this buffer will be filled by the shader later.
		void* vertex_data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_vertex_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
		std::memset(vertex_data_gpu_size, 0, quads_data_size);
		vk_device_.unmapMemory(*vk_vertex_buffer_memory_);
	}

	// Create draw indirect buffer.
	{
		const size_t num_commands= 1;
		const size_t buffer_size= sizeof(vk::DrawIndexedIndirectCommand) * num_commands;

		vk_draw_indirect_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					buffer_size,
					vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_draw_indirect_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_draw_indirect_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_draw_indirect_buffer_, *vk_draw_indirect_buffer_memory_, 0u);

		// Fill the buffer with zeros (to prevent warnings).
		// Anyway this buffer will be filled by the shader later.
		void* data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_draw_indirect_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &data_gpu_size);
		std::memset(data_gpu_size, 0, buffer_size);
		vk_device_.unmapMemory(*vk_draw_indirect_buffer_memory_);
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
		};
		vk_geometry_gen_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create pipeline layout.
	vk_geometry_gen_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*vk_geometry_gen_decriptor_set_layout_,
			0u, nullptr));

	// Create pipeline.
	vk_geometry_gen_pipeline_= vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*geometry_gen_shader_,
				"main"),
			*vk_geometry_gen_pipeline_layout_));

	// Create descriptor set pool.
	{
		const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 3u /*num descriptors*/);
		vk_geometry_gen_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					4u, // max sets.
					1u, &vk_descriptor_pool_size));
	}

	// Create descriptor set.
	vk_geometry_gen_descriptor_set_=
		std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*vk_geometry_gen_descriptor_pool_,
					1u, &*vk_geometry_gen_decriptor_set_layout_)).front());

	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_vertex_buffer_info(
			*vk_vertex_buffer_,
			0u,
			vertex_buffer_num_quads_ * sizeof(QuadVertices));

		const vk::DescriptorBufferInfo descriptor_draw_indirect_buffer_info(
			*vk_draw_indirect_buffer_,
			0u,
			1 * sizeof(vk::DrawIndexedIndirectCommand));

		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			*vk_chunk_data_buffer_,
			0u,
			c_chunk_volume);

		vk_device_.updateDescriptorSets(
			{
				{
					*vk_geometry_gen_descriptor_set_,
					GeometryGenShaderBindings::vertices_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_vertex_buffer_info,
					nullptr
				},
				{
					*vk_geometry_gen_descriptor_set_,
					GeometryGenShaderBindings::draw_indirect_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_draw_indirect_buffer_info,
					nullptr
				},
				{
					*vk_geometry_gen_descriptor_set_,
					GeometryGenShaderBindings::chunk_data_buffer,
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
}

WorldGeometryGenerator::~WorldGeometryGenerator()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldGeometryGenerator::PrepareFrame(const vk::CommandBuffer command_buffer)
{
	// Reset draw command.
	{
		vk::DrawIndexedIndirectCommand command{};
		command.indexCount= 0;
		command.instanceCount= 1;
		command.firstIndex= 0;
		command.vertexOffset= 0;
		command.firstInstance= 0;
		command_buffer.updateBuffer(*vk_draw_indirect_buffer_, 0, sizeof(command), static_cast<const void*>(&command));
	}

	// Create barrier between update buffer and its usage in shader.
	// TODO - check this is correct.
	{
		vk::BufferMemoryBarrier barrier;
		barrier.srcAccessMask= vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask= vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		barrier.size= VK_WHOLE_SIZE;
		barrier.buffer= *vk_draw_indirect_buffer_;
		// TODO - set queue family index?

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}

	// Perform geometry generation.
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *vk_geometry_gen_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*vk_geometry_gen_pipeline_layout_,
		0u,
		1u, &*vk_geometry_gen_descriptor_set_,
		0u, nullptr);

	command_buffer.dispatch(c_chunk_width, c_chunk_width, 1);
}

vk::Buffer WorldGeometryGenerator::GetVertexBuffer() const
{
	return vk_vertex_buffer_.get();
}

vk::Buffer WorldGeometryGenerator::GetDrawIndirectBuffer() const
{
	return vk_draw_indirect_buffer_.get();
}

} // namespace HexGPU
