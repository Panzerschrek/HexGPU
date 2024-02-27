#include "WorldRenderer.hpp"
#include "Assert.hpp"
#include "Constants.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"
#include <cmath>


namespace HexGPU
{

namespace
{

namespace DrawIndirectBufferBuildShaderBindings
{

// This should match bindings in the shader itself!
const uint32_t chunk_draw_info_buffer= 0;
const uint32_t draw_indirect_buffer= 1;

}

struct DrawIndirectBufferBuildUniforms
{
	int32_t world_size_chunks[2];
};

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
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_size_(world_processor.GetWorldSize())
	, geometry_generator_(window_vulkan, world_processor)
	, world_textures_manager_(window_vulkan)
{
	// Create draw indirect buffer.
	{
		const uint32_t num_commands= world_size_[0] * world_size_[1];
		const uint32_t buffer_size= uint32_t(sizeof(vk::DrawIndexedIndirectCommand)) * num_commands;

		draw_indirect_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					buffer_size,
					vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer));

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

	// Create shaders.
	draw_indirect_buffer_build_shader_= CreateShader(vk_device_, ShaderNames::world_draw_indirect_buffer_build_comp);

	// Create descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				DrawIndirectBufferBuildShaderBindings::chunk_draw_info_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
			{
				DrawIndirectBufferBuildShaderBindings::draw_indirect_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
		};
		draw_indirect_buffer_build_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create pipeline layout.
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(DrawIndirectBufferBuildUniforms));

		draw_indirect_buffer_build_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*draw_indirect_buffer_build_decriptor_set_layout_,
				1u, &push_constant_range));
	}

	// Create pipeline.
	draw_indirect_buffer_build_pipeline_= UnwrapPipeline(vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*draw_indirect_buffer_build_shader_,
				"main"),
			*draw_indirect_buffer_build_pipeline_layout_)));

	// Create descriptor set pool.
	{
		const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 2u /*num descriptors*/);
		draw_indirect_buffer_build_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					1u, // max sets.
					1u, &descriptor_pool_size));
	}

	// Create descriptor set.
	draw_indirect_buffer_build_descriptor_set_=
		std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*draw_indirect_buffer_build_descriptor_pool_,
					1u, &*draw_indirect_buffer_build_decriptor_set_layout_)).front());


	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			geometry_generator_.GetChunkDrawInfoBuffer(),
			0u,
			geometry_generator_.GetChunkDrawInfoBufferSize());

		const vk::DescriptorBufferInfo descriptor_draw_indirect_buffer_info(
			*draw_indirect_buffer_,
			0u,
			uint32_t(sizeof(vk::DrawIndexedIndirectCommand)) * world_size_[0] * world_size_[1]);

		vk_device_.updateDescriptorSets(
			{
				{
					*draw_indirect_buffer_build_descriptor_set_,
					DrawIndirectBufferBuildShaderBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
				{
					*draw_indirect_buffer_build_descriptor_set_,
					DrawIndirectBufferBuildShaderBindings::draw_indirect_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_draw_indirect_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Create shaders
	shader_vert_= CreateShader(vk_device_, ShaderNames::world_vert);
	shader_frag_= CreateShader(vk_device_, ShaderNames::world_frag);

	// Create texture sampler
	texture_sampler_=
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
	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			1u,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*texture_sampler_,
		},
	};

	decriptor_set_layout_=
		vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	// Create pipeline layout
	{
		const vk::PushConstantRange push_constant_range(
			vk::ShaderStageFlagBits::eVertex,
			0u,
			sizeof(m_Mat4));

		pipeline_layout_=
			vk_device_.createPipelineLayoutUnique(
				vk::PipelineLayoutCreateInfo(
					vk::PipelineLayoutCreateFlags(),
					1u, &*decriptor_set_layout_,
					1u, &push_constant_range));
	}

	// Create pipeline.

	const vk::PipelineShaderStageCreateInfo shader_stage_create_info[2]
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

	const vk::VertexInputBindingDescription vertex_input_binding_description(
		0u,
		sizeof(WorldVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vertex_input_attribute_description[]
	{
		{0u, 0u, vk::Format::eR16G16B16A16Sscaled, 0u},
		{1u, 0u, vk::Format::eR16G16B16A16Sint, sizeof(int16_t) * 4},
	};

	const vk::PipelineVertexInputStateCreateInfo pipiline_vertex_input_state_create_info(
		vk::PipelineVertexInputStateCreateFlags(),
		1u, &vertex_input_binding_description,
		uint32_t(std::size(vertex_input_attribute_description)), vertex_input_attribute_description);

	const vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList);

	const vk::Extent2D viewport_size= window_vulkan.GetViewportSize();
	const vk::Viewport viewport(0.0f, 0.0f, float(viewport_size.width), float(viewport_size.height), 0.0f, 1.0f);
	const vk::Rect2D scissor(vk::Offset2D(0, 0), viewport_size);

	const vk::PipelineViewportStateCreateInfo pipieline_viewport_state_create_info(
		vk::PipelineViewportStateCreateFlags(),
		1u, &viewport,
		1u, &scissor);

	const vk::PipelineRasterizationStateCreateInfo pipilane_rasterization_state_create_info(
		vk::PipelineRasterizationStateCreateFlags(),
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack, // Use back-face culling.
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info;

	const vk::PipelineDepthStencilStateCreateInfo pipeline_depth_state_create_info(
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

	const vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(
		VK_FALSE,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

	const vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info(
		vk::PipelineColorBlendStateCreateFlags(),
		VK_FALSE,
		vk::LogicOp::eCopy,
		1u, &pipeline_color_blend_attachment_state);

	pipeline_=
		UnwrapPipeline(vk_device_.createGraphicsPipelineUnique(
			nullptr,
			vk::GraphicsPipelineCreateInfo(
				vk::PipelineCreateFlags(),
				uint32_t(std::size(shader_stage_create_info)),
				shader_stage_create_info,
				&pipiline_vertex_input_state_create_info,
				&pipeline_input_assembly_state_create_info,
				nullptr,
				&pipieline_viewport_state_create_info,
				&pipilane_rasterization_state_create_info,
				&pipeline_multisample_state_create_info,
				&pipeline_depth_state_create_info,
				&pipeline_color_blend_state_create_info,
				nullptr,
				*pipeline_layout_,
				window_vulkan.GetRenderPass(),
				0u)));

	const auto memory_properties= window_vulkan.GetMemoryProperties();

	// Create index buffer.
	{
		const std::vector<uint16_t> world_indeces= GetQuadsIndices();

		const size_t indices_size= world_indeces.size() * sizeof(uint16_t);

		index_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					indices_size,
					vk::BufferUsageFlagBits::eIndexBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*index_buffer_);

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= i;
				break;
			}
		}

		index_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*index_buffer_, *index_buffer_memory_, 0u);

		void* vertex_data_gpu_size= nullptr;
		vk_device_.mapMemory(*index_buffer_memory_, 0u, memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
		std::memcpy(vertex_data_gpu_size, world_indeces.data(), indices_size);
		vk_device_.unmapMemory(*index_buffer_memory_);
	}

	// Create descriptor set pool.
	const vk::DescriptorPoolSize descriptor_pool_size(vk::DescriptorType::eCombinedImageSampler, 1u);
	descriptor_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				1u, // max sets.
				1u, &descriptor_pool_size));

	// Create descriptor set.
	descriptor_set_=
		std::move(
		vk_device_.allocateDescriptorSetsUnique(
			vk::DescriptorSetAllocateInfo(
				*descriptor_pool_,
				1u, &*decriptor_set_layout_)).front());

	// Update descriptor set.
	{
		const vk::DescriptorImageInfo descriptor_tex_info(
			vk::Sampler(),
			world_textures_manager_.GetImageView(),
			vk::ImageLayout::eShaderReadOnlyOptimal);

		vk_device_.updateDescriptorSets(
			{
				{
					*descriptor_set_,
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
	world_textures_manager_.PrepareFrame(command_buffer);
	geometry_generator_.PrepareFrame(command_buffer);
	BuildDrawIndirectBuffer(command_buffer);
}

void WorldRenderer::Draw(const vk::CommandBuffer command_buffer, const m_Mat4& view_matrix)
{
	const vk::Buffer vertex_buffer= geometry_generator_.GetVertexBuffer();

	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &vertex_buffer, &offsets);
	command_buffer.bindIndexBuffer(*index_buffer_, 0u, vk::IndexType::eUint16);
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*pipeline_layout_,
		0u,
		1u, &*descriptor_set_,
		0u, nullptr);

	const m_Vec3 scale_vec(0.5f / std::sqrt(3.0f), 0.5f, 1.0f );
	m_Mat4 scale_mat;
	scale_mat.Scale(scale_vec);
	const m_Mat4 final_mat= scale_mat * view_matrix;

	command_buffer.pushConstants(*pipeline_layout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(view_matrix), &final_mat);

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);

	command_buffer.drawIndexedIndirect(
		*draw_indirect_buffer_,
		0,
		world_size_[0] * world_size_[1],
		sizeof(vk::DrawIndexedIndirectCommand));
}

void WorldRenderer::BuildDrawIndirectBuffer(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *draw_indirect_buffer_build_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*draw_indirect_buffer_build_pipeline_layout_,
		0u,
		1u, &*draw_indirect_buffer_build_descriptor_set_,
		0u, nullptr);

	DrawIndirectBufferBuildUniforms uniforms;
	uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
	uniforms.world_size_chunks[1]= int32_t(world_size_[1]);

	command_buffer.pushConstants(
		*draw_indirect_buffer_build_pipeline_layout_,
		vk::ShaderStageFlagBits::eCompute,
		0,
		sizeof(DrawIndirectBufferBuildUniforms),
		&uniforms);

	// Dispatch a thread for each chunk.
	command_buffer.dispatch(world_size_[0], world_size_[1], 1);

	// Create barrier between update indirect draw buffer and its usage for rendering.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead,
			queue_family_index_, queue_family_index_,
			*draw_indirect_buffer_,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eDrawIndirect,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

} // namespace HexGPU
