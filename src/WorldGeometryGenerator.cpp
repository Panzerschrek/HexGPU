#include "WorldGeometryGenerator.hpp"
#include "Assert.hpp"
#include "ShaderList.hpp"

namespace HexGPU
{


WorldGeometryGenerator::WorldGeometryGenerator(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
{
	// Create shaders.
	geometry_gen_shader_= CreateShader(vk_device_, ShaderNames::geometry_gen_comp);

	// Create vertex buffer.
	{
		const size_t quads_data_size= g_quad_grid_size[0] * g_quad_grid_size[1] * sizeof(QuadVertices);

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

	// Create descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[1]
		{
			{
				0u,
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
		const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1u);
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
		const vk::DescriptorBufferInfo descriptor_buffer_info(
			*vk_vertex_buffer_,
			0u,
			g_quad_grid_size[0] * g_quad_grid_size[1] * sizeof(QuadVertices));

		vk_device_.updateDescriptorSets(
			{
				{
					*vk_geometry_gen_descriptor_set_,
					0u,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_buffer_info,
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
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *vk_geometry_gen_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*vk_geometry_gen_pipeline_layout_,
		0u,
		1u, &*vk_geometry_gen_descriptor_set_,
		0u, nullptr);

	command_buffer.dispatch(g_quad_grid_size[0], g_quad_grid_size[1], 1);
}

vk::Buffer WorldGeometryGenerator::GetVertexBuffer() const
{
	return vk_vertex_buffer_.get();
}

} // namespace HexGPU
