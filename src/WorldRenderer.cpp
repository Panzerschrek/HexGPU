#include "WorldRenderer.hpp"
#include "Assert.hpp"
#include "Constants.hpp"
#include "ShaderList.hpp"
#include <cmath>
#include <cstring>


namespace HexGPU
{

namespace
{

// Returns indeces for quads with size - maximum uint16_t vertex index.
std::vector<uint16_t> GetQuadsIndices()
{
	// Each quad contains 4 unique vertices.
	// So, whe have maximum number of quads equal to maximal possible index devided by 4.
	// Add for sequrity extra -1.
	size_t quad_count= 65536 / 4 - 1;
	std::vector<uint16_t> indeces( quad_count * 6 );

	for(uint32_t i= 0, v= 0; i< quad_count * 6; i+= 6, v+= 4)
	{
		indeces[i+0]= uint16_t(v + 0);
		indeces[i+1]= uint16_t(v + 1);
		indeces[i+2]= uint16_t(v + 2);
		indeces[i+3]= uint16_t(v + 0);
		indeces[i+4]= uint16_t(v + 2);
		indeces[i+5]= uint16_t(v + 3);
	}

	return indeces;
}

using QuadVertices= std::array<WorldVertex, 4>;

} // namespace

WorldRenderer::WorldRenderer(WindowVulkan& window_vulkan, WorldProcessor& world_processor)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, vk_queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, geometry_generator_(window_vulkan, world_processor)
	, world_textures_manager_(window_vulkan)
{
	// Create shaders
	shader_vert_= CreateShader(vk_device_, ShaderNames::triangle_vert);
	shader_frag_= CreateShader(vk_device_, ShaderNames::triangle_frag);

	// Create texture sampler
	vk_texture_sampler_=
		vk_device_.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eLinear,
				vk::Filter::eLinear,
				vk::SamplerMipmapMode::eLinear,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				0.0f,
				VK_FALSE, // anisotropy
				1.0f, // anisotropy level
				VK_FALSE,
				vk::CompareOp::eNever,
				0.0f,
				100.0f,
				vk::BorderColor::eFloatTransparentBlack,
				VK_FALSE));

	// Create descriptor set layout.
	const vk::DescriptorSetLayoutBinding vk_descriptor_set_layout_bindings[]
	{
		{
			0u,
			vk::DescriptorType::eUniformBuffer,
			1u,
			vk::ShaderStageFlagBits::eVertex
		},
		{
			1u,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*vk_texture_sampler_,
		},
	};

	vk_decriptor_set_layout_=
		vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(vk_descriptor_set_layout_bindings)), vk_descriptor_set_layout_bindings));

	const vk::PushConstantRange vk_push_constant_range(
		vk::ShaderStageFlagBits::eVertex,
		0u,
		sizeof(m_Mat4));

	// Create pipeline layout
	vk_pipeline_layout_=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u,
				&*vk_decriptor_set_layout_,
				1u,
				&vk_push_constant_range));

	// Create pipeline.

	const vk::PipelineShaderStageCreateInfo vk_shader_stage_create_info[2]
	{
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eVertex,
			*shader_vert_,
			"main"
		},
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eFragment,
			*shader_frag_,
			"main"
		},
	};

	const vk::VertexInputBindingDescription vk_vertex_input_binding_description(
		0u,
		sizeof(WorldVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vk_vertex_input_attribute_description[2]
	{
		{0u, 0u, vk::Format::eR16G16B16A16Sscaled, 0u},
		{1u, 0u, vk::Format::eR16G16B16A16Sscaled, sizeof(int16_t) * 4},
	};

	const vk::PipelineVertexInputStateCreateInfo vk_pipiline_vertex_input_state_create_info(
		vk::PipelineVertexInputStateCreateFlags(),
		1u, &vk_vertex_input_binding_description,
		uint32_t(std::size(vk_vertex_input_attribute_description)), vk_vertex_input_attribute_description);

	const vk::PipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList);

	const vk::Extent2D viewport_size= window_vulkan.GetViewportSize();
	const vk::Viewport vk_viewport(0.0f, 0.0f, float(viewport_size.width), float(viewport_size.height), 0.0f, 1.0f);
	const vk::Rect2D vk_scissor(vk::Offset2D(0, 0), viewport_size);

	const vk::PipelineViewportStateCreateInfo vk_pipieline_viewport_state_create_info(
		vk::PipelineViewportStateCreateFlags(),
		1u, &vk_viewport,
		1u, &vk_scissor);

	const vk::PipelineRasterizationStateCreateInfo vk_pipilane_rasterization_state_create_info(
		vk::PipelineRasterizationStateCreateFlags(),
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack, // Use back-face culling.
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info;

	const vk::PipelineDepthStencilStateCreateInfo vk_pipeline_depth_state_create_info(
		vk::PipelineDepthStencilStateCreateFlags(),
		VK_TRUE,
		VK_TRUE,
		vk::CompareOp::eLess,
		VK_FALSE,
		VK_FALSE,
		vk::StencilOpState(),
		vk::StencilOpState(),
		0.0f,
		1.0f);

	const vk::PipelineColorBlendAttachmentState vk_pipeline_color_blend_attachment_state(
		VK_FALSE,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

	const vk::PipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info(
		vk::PipelineColorBlendStateCreateFlags(),
		VK_FALSE,
		vk::LogicOp::eCopy,
		1u, &vk_pipeline_color_blend_attachment_state);

	vk_pipeline_=
		vk_device_.createGraphicsPipelineUnique(
			nullptr,
			vk::GraphicsPipelineCreateInfo(
				vk::PipelineCreateFlags(),
				uint32_t(std::size(vk_shader_stage_create_info)),
				vk_shader_stage_create_info,
				&vk_pipiline_vertex_input_state_create_info,
				&vk_pipeline_input_assembly_state_create_info,
				nullptr,
				&vk_pipieline_viewport_state_create_info,
				&vk_pipilane_rasterization_state_create_info,
				&vk_pipeline_multisample_state_create_info,
				&vk_pipeline_depth_state_create_info,
				&vk_pipeline_color_blend_state_create_info,
				nullptr,
				*vk_pipeline_layout_,
				window_vulkan.GetRenderPass(),
				0u));

	const auto memory_properties= window_vulkan.GetMemoryProperties();

	// Create index buffer.
	{
		const std::vector<uint16_t> world_indeces= GetQuadsIndices();

		const size_t indices_size= world_indeces.size() * sizeof(uint16_t);

		vk_index_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					indices_size,
					vk::BufferUsageFlagBits::eIndexBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_index_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_index_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_index_buffer_, *vk_index_buffer_memory_, 0u);

		void* vertex_data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_index_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
		std::memcpy(vertex_data_gpu_size, world_indeces.data(), indices_size);
		vk_device_.unmapMemory(*vk_index_buffer_memory_);
	}

	// Create descriptor set pool.
	const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eCombinedImageSampler, 1u);
	vk_descriptor_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				4u, // max sets.
				1u, &vk_descriptor_pool_size));

	// Create descriptor set.
	vk_descriptor_set_=
		std::move(
		vk_device_.allocateDescriptorSetsUnique(
			vk::DescriptorSetAllocateInfo(
				*vk_descriptor_pool_,
				1u, &*vk_decriptor_set_layout_)).front());

	// Update descriptor set.
	{
		const vk::DescriptorImageInfo descriptor_tex_info(
			vk::Sampler(),
			world_textures_manager_.GetImageView(),
			vk::ImageLayout::eGeneral);

		vk_device_.updateDescriptorSets(
			{
				{
					*vk_descriptor_set_,
					1u,
					0u,
					1u,
					vk::DescriptorType::eCombinedImageSampler,
					&descriptor_tex_info,
					nullptr,
					nullptr
				},
			},
			{});
	}

}

WorldRenderer::~WorldRenderer()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldRenderer::PrepareFrame(const vk::CommandBuffer command_buffer)
{
	geometry_generator_.PrepareFrame(command_buffer);

	const vk::Buffer vertex_buffer= geometry_generator_.GetVertexBuffer();

	// Create barrier between update vertex buffer and its usage for rendering.
	// TODO - check this is correct.
	{
		vk::BufferMemoryBarrier barrier;
		barrier.srcAccessMask= vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		barrier.dstAccessMask= vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		barrier.size= VK_WHOLE_SIZE;
		barrier.buffer= vertex_buffer;
		barrier.srcQueueFamilyIndex= vk_queue_family_index_;
		barrier.dstQueueFamilyIndex= vk_queue_family_index_;

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eVertexShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}

	const vk::Buffer draw_indirect_buffer= geometry_generator_.GetDrawIndirectBuffer();

	// Create barrier between update indirect draw buffer and its usage for rendering.
	// TODO - check this is correct.
	{
		vk::BufferMemoryBarrier barrier;
		barrier.srcAccessMask= vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		barrier.dstAccessMask= vk::AccessFlagBits::eIndirectCommandRead;
		barrier.size= VK_WHOLE_SIZE;
		barrier.buffer= draw_indirect_buffer;
		barrier.srcQueueFamilyIndex= vk_queue_family_index_;
		barrier.dstQueueFamilyIndex= vk_queue_family_index_;

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eDrawIndirect,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

void WorldRenderer::Draw(const vk::CommandBuffer command_buffer, const m_Mat4& view_matrix)
{
	const vk::Buffer vertex_buffer= geometry_generator_.GetVertexBuffer();
	const vk::Buffer draw_indirect_buffer= geometry_generator_.GetDrawIndirectBuffer();

	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &vertex_buffer, &offsets);
	command_buffer.bindIndexBuffer(*vk_index_buffer_, 0u, vk::IndexType::eUint16);
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*vk_pipeline_layout_,
		0u,
		1u, &*vk_descriptor_set_,
		0u, nullptr);

	const m_Vec3 scale_vec(0.5f / std::sqrt(3.0f), 0.5f, 1.0f );
	m_Mat4 scale_mat;
	scale_mat.Scale(scale_vec);
	const m_Mat4 final_mat= scale_mat * view_matrix;

	command_buffer.pushConstants(*vk_pipeline_layout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(view_matrix), &final_mat);

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *vk_pipeline_);

	command_buffer.drawIndexedIndirect(
		draw_indirect_buffer,
		0,
		c_chunk_matrix_size[0] * c_chunk_matrix_size[1],
		sizeof(vk::DrawIndexedIndirectCommand));
}

} // namespace HexGPU
