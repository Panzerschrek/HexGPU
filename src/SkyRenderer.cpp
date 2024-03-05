#include "SkyRenderer.hpp"
#include "GlobalDescriptorPool.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

struct DrawUniforms
{
	float view_matrix[16]{};
};


GraphicsPipeline CreateSkyPipeline(
	const vk::Device vk_device,
	const vk::Extent2D viewport_size,
	const vk::RenderPass render_pass)
{
	GraphicsPipeline pipeline;

	pipeline.shader_vert= CreateShader(vk_device, ShaderNames::sky_vert);
	pipeline.shader_frag= CreateShader(vk_device, ShaderNames::sky_frag);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			0u,
			vk::DescriptorType::eUniformBuffer,
			1u,
			vk::ShaderStageFlagBits::eVertex,
		},
	};

	pipeline.descriptor_set_layout=
		vk_device.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	pipeline.pipeline_layout=
		vk_device.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*pipeline.descriptor_set_layout,
				0u, nullptr));

	const vk::PipelineShaderStageCreateInfo shader_stage_create_info[2]
	{
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eVertex,
			*pipeline.shader_vert,
			"main"
		},
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eFragment,
			*pipeline.shader_frag,
			"main"
		},
	};

	const vk::PipelineVertexInputStateCreateInfo pipiline_vertex_input_state_create_info(
		vk::PipelineVertexInputStateCreateFlags(),
		0u, nullptr,
		0u, nullptr);

	const vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList);

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
		VK_FALSE, 0.0f, 0.0f, 0.0f, // Depth bias
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

	pipeline.pipeline=
		UnwrapPipeline(vk_device.createGraphicsPipelineUnique(
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
				*pipeline.pipeline_layout,
				render_pass,
				0u)));

	return pipeline;
}

} // namespace

SkyRenderer::SkyRenderer(
	WindowVulkan& window_vulkan,
	const WorldProcessor& world_processor,
	const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_processor_(world_processor)
	, uniform_buffer_(
		window_vulkan,
		sizeof(DrawUniforms),
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, pipeline_(
		CreateSkyPipeline(
			window_vulkan.GetVulkanDevice(),
			window_vulkan.GetViewportSize(),
			window_vulkan.GetRenderPass()))
	, descriptor_set_(CreateDescriptorSet(vk_device_, global_descriptor_pool, *pipeline_.descriptor_set_layout))
{
	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_uniform_buffer_info(
			uniform_buffer_.GetBuffer(),
			0u,
			sizeof(DrawUniforms));

		vk_device_.updateDescriptorSets(
			{
				{
					descriptor_set_,
					0u,
					0u,
					1u,
					vk::DescriptorType::eUniformBuffer,
					nullptr,
					&descriptor_uniform_buffer_info,
					nullptr
				},
			},
			{});
	}
}

SkyRenderer::~SkyRenderer()
{
	// Sync before destruction
	vk_device_.waitIdle();
}

void SkyRenderer::PrepareFrame(const vk::CommandBuffer command_buffer)
{
	// Copy view matrix.
	command_buffer.copyBuffer(
		world_processor_.GetPlayerStateBuffer(),
		uniform_buffer_.GetBuffer(),
		{
			{
				offsetof(WorldProcessor::PlayerState, sky_matrix),
				offsetof(DrawUniforms, view_matrix),
				sizeof(float) * 16
			},
		});

	// Add barrier between uniform buffer memory copy and result usage in shader.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
			queue_family_index_, queue_family_index_,
			uniform_buffer_.GetBuffer(),
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eVertexShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

void SkyRenderer::Draw(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*pipeline_.pipeline_layout,
		0u,
		{descriptor_set_},
		{});

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_.pipeline);

	const uint32_t c_num_vertices= 6u * 2u * 3u; // This must match the corresponding contant in GLSL code!

	command_buffer.draw(c_num_vertices, 1u, 0u, 0u);
}

} // namespace HexGPU