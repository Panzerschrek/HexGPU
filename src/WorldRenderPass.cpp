#include "WorldRenderPass.hpp"
#include "GlobalDescriptorPool.hpp"
#include "Log.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

vk::Extent3D GetFramebufferTextureSize(WindowVulkan& window_vulkan)
{
	const vk::Extent2D size_2d= window_vulkan.GetViewportSize();
	return vk::Extent3D(size_2d.width, size_2d.height, 1);
}

vk::Format ChooseDepthFormat(const vk::PhysicalDevice physical_device)
{
	static constexpr vk::Format depth_formats[]
	{
		// Depth formats by priority.
		vk::Format::eD32Sfloat,
		vk::Format::eD24UnormS8Uint,
		vk::Format::eX8D24UnormPack32,
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD16Unorm,
		vk::Format::eD16UnormS8Uint,
	};

	for(const vk::Format depth_format_candidate : depth_formats)
	{
		const vk::FormatProperties format_properties=
			physical_device.getFormatProperties(depth_format_candidate);

		const vk::FormatFeatureFlags required_falgs= vk::FormatFeatureFlagBits::eDepthStencilAttachment;
		if((format_properties.optimalTilingFeatures & required_falgs) == required_falgs)
		{
			Log::Info("Framebuffer depth format: ", vk::to_string(depth_format_candidate));
			return depth_format_candidate;
		}
	}

	Log::Warning("Can't choose proper depth format, using ", vk::to_string(vk::Format::eD16Unorm));
	return vk::Format::eD16Unorm;
}

vk::UniqueRenderPass CreateRenderPass(
	const vk::Device vk_device,
	const vk::Format depth_format,
	const vk::SampleCountFlagBits samples)
{
	const vk::AttachmentDescription attachment_descriptions[]
	{
		{
			vk::AttachmentDescriptionFlags(),
			vk::Format::eR8G8B8A8Unorm,
			samples,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal // Leave in this layout.
		},
		{
			vk::AttachmentDescriptionFlags(),
			depth_format,
			samples,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal, // Leane in this layout.
		},
	};

	const vk::AttachmentReference attachment_reference_color(0u, vk::ImageLayout::eColorAttachmentOptimal);
	const vk::AttachmentReference attachment_reference_depth(1u, vk::ImageLayout::eDepthStencilAttachmentOptimal);

	const vk::SubpassDescription subpass_description(
		vk::SubpassDescriptionFlags(),
		vk::PipelineBindPoint::eGraphics,
		0u, nullptr,
		1u, &attachment_reference_color,
		0u,
		&attachment_reference_depth);

	return vk_device.createRenderPassUnique(
			vk::RenderPassCreateInfo(
				vk::RenderPassCreateFlags(),
				uint32_t(std::size(attachment_descriptions)), attachment_descriptions,
				1u, &subpass_description));
}

vk::UniqueFramebuffer CreateFramebuffer(
	const vk::Device vk_device,
	const vk::ImageView image_view,
	const vk::ImageView depth_image_view,
	const vk::RenderPass render_pass,
	const vk::Extent3D framebuffer_size)
{
	const vk::ImageView attachments[]{image_view, depth_image_view};
	return vk_device.createFramebufferUnique(
		vk::FramebufferCreateInfo(
			vk::FramebufferCreateFlags(),
			render_pass,
			uint32_t(std::size(attachments)), attachments,
			framebuffer_size.width, framebuffer_size.height, framebuffer_size.depth));
}

GraphicsPipeline CreateWorldRenderPassPresentPipeline(
	const vk::Device vk_device,
	const vk::RenderPass swapchain_render_pass,
	const vk::Sampler sampler,
	const vk::Extent2D viewport_size)
{
	GraphicsPipeline pipeline;

	pipeline.shader_vert= CreateShader(vk_device, ShaderNames::world_render_pass_present_vert);
	pipeline.shader_frag= CreateShader(vk_device, ShaderNames::world_render_pass_present_frag);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			0u,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&sampler,
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
		vk::CullModeFlagBits::eNone,
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info;

	const vk::PipelineDepthStencilStateCreateInfo pipeline_depth_state_create_info(
		vk::PipelineDepthStencilStateCreateFlags(),
		VK_FALSE, // Do not use depth test.
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
				swapchain_render_pass,
				0u)));

	return pipeline;
}

} // namespace

WorldRenderPass::WorldRenderPass(WindowVulkan& window_vulkan, const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, samples_(vk::SampleCountFlagBits::e4)
	, framebuffer_size_(GetFramebufferTextureSize(window_vulkan))
	, image_(vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			framebuffer_size_,
			1u,
			1u,
			samples_,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			vk::SharingMode::eExclusive,
			0u, nullptr,
			vk::ImageLayout::eUndefined)))
	, image_memory_(AllocateAndBindImageMemory(vk_device_, *image_, window_vulkan.GetMemoryProperties()))
	, image_view_(vk_device_.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			*image_,
			vk::ImageViewType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u))))
	, depth_format_(ChooseDepthFormat(window_vulkan.GetPhysicalDevice()))
	, depth_image_(vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			depth_format_,
			framebuffer_size_,
			1u,
			1u,
			samples_,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
			vk::SharingMode::eExclusive,
			0u, nullptr,
			vk::ImageLayout::eUndefined)))
	, depth_image_memory_(AllocateAndBindImageMemory(vk_device_, *depth_image_, window_vulkan.GetMemoryProperties()))
	, depth_image_view_(vk_device_.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			*depth_image_,
			vk::ImageViewType::e2D,
			depth_format_,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0u, 1u, 0u, 1u))))
	, render_pass_(CreateRenderPass(vk_device_, depth_format_, samples_))
	, framebuffer_(CreateFramebuffer(vk_device_, *image_view_, *depth_image_view_, *render_pass_, framebuffer_size_))
	, sampler_(vk_device_.createSamplerUnique(
		vk::SamplerCreateInfo(
			vk::SamplerCreateFlags(),
			vk::Filter::eNearest,
			vk::Filter::eNearest,
			vk::SamplerMipmapMode::eNearest,
			vk::SamplerAddressMode::eClampToEdge,
			vk::SamplerAddressMode::eClampToBorder,
			vk::SamplerAddressMode::eClampToEdge,
			0.0f,
			VK_FALSE,
			1.0f,
			VK_FALSE,
			vk::CompareOp::eNever,
			0.0f,
			0.0f,
			vk::BorderColor::eFloatTransparentBlack,
			VK_FALSE)))
	, pipeline_(
		CreateWorldRenderPassPresentPipeline(
			vk_device_,
			window_vulkan.GetRenderPass(),
			*sampler_,
			vk::Extent2D(framebuffer_size_.width, framebuffer_size_.height)))
	, descriptor_set_(CreateDescriptorSet(vk_device_, global_descriptor_pool, *pipeline_.descriptor_set_layout))
{
	// Update descriptor set.
	{
		const vk::DescriptorImageInfo descriptor_image_info(
			vk::Sampler(),
			*image_view_,
			vk::ImageLayout::eShaderReadOnlyOptimal);

		vk_device_.updateDescriptorSets(
			{
				{
					descriptor_set_,
					0u,
					0u,
					1u,
					vk::DescriptorType::eCombinedImageSampler,
					&descriptor_image_info,
					nullptr,
					nullptr
				},
			},
			{});
	}
}

WorldRenderPass::~WorldRenderPass()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

vk::SampleCountFlagBits WorldRenderPass::GetSamples() const
{
	return samples_;
}

vk::Framebuffer WorldRenderPass::GetFramebuffer() const
{
	return *framebuffer_;
}

vk::Extent2D WorldRenderPass::GetFramebufferSize() const
{
	return vk::Extent2D(framebuffer_size_.width, framebuffer_size_.height);
}

vk::RenderPass WorldRenderPass::GetRenderPass() const
{
	return *render_pass_;
}

void WorldRenderPass::CollectPassOutputs(TaskOrganizer::GraphicsTaskParams& out_task_params) const
{
	out_task_params.output_color_images.push_back(TaskOrganizer::ImageInfo{*image_, vk::ImageAspectFlagBits::eColor, 1, 1});
	out_task_params.output_depth_images.push_back(TaskOrganizer::ImageInfo{*depth_image_, vk::ImageAspectFlagBits::eDepth, 1, 1});
}

void WorldRenderPass::CollectFrameInputs(TaskOrganizer::GraphicsTaskParams& out_task_params) const
{
	out_task_params.input_images.push_back(TaskOrganizer::ImageInfo{*image_, vk::ImageAspectFlagBits::eColor, 1, 1});
}

void WorldRenderPass::Draw(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*pipeline_.pipeline_layout,
		0u,
		{descriptor_set_},
		{});

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_.pipeline);

	const uint32_t c_num_vertices= 3u; // This must match the corresponding constant in GLSL code!

	command_buffer.draw(c_num_vertices, 1u, 0u, 0u);
}

} // namespace HexGPU
