#include "SkyRenderer.hpp"
#include "GlobalDescriptorPool.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

namespace CloudsShaderBindings
{
	const ShaderBindingIndex uniforms_buffer= 0;
	const ShaderBindingIndex texture_sampler= 1;
}

struct SkyShaderUniforms
{
	float view_matrix[16]{};
	float sky_color[4]{};
	float sun_direction[4]{};
	float clouds_color[4]{};
};

struct CloudsUniforms
{
	float tex_coord_shift[2]{};
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
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
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

	const vk::PipelineShaderStageCreateInfo shader_stage_create_info[]
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

	// No input vertices - generate coordinates in vertex shader.
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
		VK_FALSE, 0.0f, 0.0f, 0.0f,
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info;

	const vk::PipelineDepthStencilStateCreateInfo pipeline_depth_state_create_info(
		vk::PipelineDepthStencilStateCreateFlags(),
		VK_TRUE,
		VK_FALSE, // Do not bother writing depth.
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
	, clouds_texture_generator_(window_vulkan, global_descriptor_pool)
	, uniform_buffer_(
		window_vulkan,
		sizeof(SkyShaderUniforms),
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, skybox_pipeline_(
		CreateSkyPipeline(
			window_vulkan.GetVulkanDevice(),
			window_vulkan.GetViewportSize(),
			window_vulkan.GetRenderPass()))
	, skybox_descriptor_set_(CreateDescriptorSet(vk_device_, global_descriptor_pool, *skybox_pipeline_.descriptor_set_layout))
	, clouds_pipeline_(
		CreateCloudsPipeline(
			window_vulkan.GetVulkanDevice(),
			window_vulkan.GetViewportSize(),
			window_vulkan.GetRenderPass()))
	, clouds_descriptor_set_(CreateDescriptorSet(vk_device_, global_descriptor_pool, *clouds_pipeline_.descriptor_set_layout))
{
	// Update skybox descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_uniform_buffer_info(
			uniform_buffer_.GetBuffer(),
			0u,
			sizeof(SkyShaderUniforms));

		vk_device_.updateDescriptorSets(
			{
				{
					skybox_descriptor_set_,
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

	// Update clouds descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_uniform_buffer_info(
			uniform_buffer_.GetBuffer(),
			0u,
			sizeof(SkyShaderUniforms));

		const vk::DescriptorImageInfo descriptor_tex_info(
			vk::Sampler(),
			clouds_texture_generator_.GetCloudsImageView(),
			vk::ImageLayout::eShaderReadOnlyOptimal);

		vk_device_.updateDescriptorSets(
			{
				{
					clouds_descriptor_set_,
					CloudsShaderBindings::uniforms_buffer,
					0u,
					1u,
					vk::DescriptorType::eUniformBuffer,
					nullptr,
					&descriptor_uniform_buffer_info,
					nullptr
				},
				{
					clouds_descriptor_set_,
					CloudsShaderBindings::texture_sampler,
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

SkyRenderer::~SkyRenderer()
{
	// Sync before destruction
	vk_device_.waitIdle();
}

void SkyRenderer::PrepareFrame(TaskOrganizer& task_organizer)
{
	clouds_texture_generator_.PrepareFrame(task_organizer);

	TaskOrganizer::TransferTaskParams task;
	task.input_buffers.push_back(world_processor_.GetPlayerStateBuffer());
	task.input_buffers.push_back(world_processor_.GetWorldGlobalStateBuffer());
	task.output_buffers.push_back(uniform_buffer_.GetBuffer());

	const auto task_func=
		[this](const vk::CommandBuffer command_buffer)
		{
			// Copy view matrix.
			command_buffer.copyBuffer(
				world_processor_.GetPlayerStateBuffer(),
				uniform_buffer_.GetBuffer(),
				{
					{
						offsetof(WorldProcessor::PlayerState, sky_matrix),
						offsetof(SkyShaderUniforms, view_matrix),
						sizeof(float) * 16
					},
				});

			// Copy various params from global world state.
			command_buffer.copyBuffer(
				world_processor_.GetWorldGlobalStateBuffer(),
				uniform_buffer_.GetBuffer(),
				{
					{
						offsetof(WorldProcessor::WorldGlobalState, sky_color),
						offsetof(SkyShaderUniforms, sky_color),
						sizeof(float) * 4
					},
					{
						offsetof(WorldProcessor::WorldGlobalState, sun_direction),
						offsetof(SkyShaderUniforms, sun_direction),
						sizeof(float) * 4
					},
					{
						offsetof(WorldProcessor::WorldGlobalState, clouds_color),
						offsetof(SkyShaderUniforms, clouds_color),
						sizeof(float) * 4
					},
				});
		};

	task_organizer.ExecuteTask(task, task_func);
}

void SkyRenderer::CollectFrameInputs(TaskOrganizer::GraphicsTaskParams& out_task_params)
{
	out_task_params.uniform_buffers.push_back(uniform_buffer_.GetBuffer());
	out_task_params.input_images.push_back(clouds_texture_generator_.GetImageInfo());
}

void SkyRenderer::Draw(const vk::CommandBuffer command_buffer, const float time_s)
{
	DrawSkybox(command_buffer);
	DrawClouds(command_buffer, time_s);
}

void SkyRenderer::DrawSkybox(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*skybox_pipeline_.pipeline_layout,
		0u,
		{skybox_descriptor_set_},
		{});

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *skybox_pipeline_.pipeline);

	const uint32_t c_num_vertices= 6u * 3u; // This must match the corresponding constant in GLSL code!

	command_buffer.draw(c_num_vertices, 1u, 0u, 0u);
}

void SkyRenderer::DrawClouds(const vk::CommandBuffer command_buffer, const float time_s)
{
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*clouds_pipeline_.pipeline_layout,
		0u,
		{clouds_descriptor_set_},
		{});

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *clouds_pipeline_.pipeline);

	CloudsUniforms uniforms;
	uniforms.tex_coord_shift[0]= -time_s / 320.0f;
	uniforms.tex_coord_shift[1]= 0.0f;

	command_buffer.pushConstants(
		*clouds_pipeline_.pipeline_layout,
		vk::ShaderStageFlagBits::eFragment,
		0,
		sizeof(CloudsUniforms), static_cast<const void*>(&uniforms));

	const uint32_t c_num_vertices= 4u * 3u; // This must match the corresponding constant in GLSL code!

	command_buffer.draw(c_num_vertices, 1u, 0u, 0u);
}

SkyRenderer::CloudsPipeline SkyRenderer::CreateCloudsPipeline(
	const vk::Device vk_device,
	const vk::Extent2D viewport_size,
	const vk::RenderPass render_pass)
{
	CloudsPipeline pipeline;

	pipeline.shader_vert= CreateShader(vk_device, ShaderNames::clouds_vert);
	pipeline.shader_frag= CreateShader(vk_device, ShaderNames::clouds_frag);

	pipeline.texture_sampler=
		vk_device.createSamplerUnique(
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

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			CloudsShaderBindings::uniforms_buffer,
			vk::DescriptorType::eUniformBuffer,
			1u,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		},
		{
			CloudsShaderBindings::texture_sampler,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*pipeline.texture_sampler,
		},
	};

	pipeline.descriptor_set_layout=
		vk_device.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eFragment,
		0u,
		sizeof(CloudsUniforms));

	pipeline.pipeline_layout=
		vk_device.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*pipeline.descriptor_set_layout,
				1u, &push_constant_range));

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
		VK_FALSE, 0.0f, 0.0f, 0.0f,
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info;

	const vk::PipelineDepthStencilStateCreateInfo pipeline_depth_state_create_info(
		vk::PipelineDepthStencilStateCreateFlags(),
		VK_TRUE,
		VK_FALSE, // Do not bother writing depth.
		vk::CompareOp::eLess,
		VK_FALSE,
		VK_FALSE,
		vk::StencilOpState(),
		vk::StencilOpState(),
		0.0f,
		1.0f);

	// Use simple alpha-blending.
	const vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(
		VK_TRUE,
		vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
		vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
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

} // namespace HexGPU
