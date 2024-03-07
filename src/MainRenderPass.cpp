#include "MainRenderPass.hpp"
#include "Log.hpp"

namespace HexGPU
{

MainRenderPass::MainRenderPass(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, viewport_size_(window_vulkan.GetViewportSize())
{
	const auto physical_device= window_vulkan.GetPhysicalDevice();
	const auto memory_properties= window_vulkan.GetMemoryProperties();

	// Select depth buffer format.
	const vk::Format depth_formats[]
	{
		// Depth formats by priority.
		vk::Format::eD32Sfloat,
		vk::Format::eD24UnormS8Uint,
		vk::Format::eX8D24UnormPack32,
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD16Unorm,
		vk::Format::eD16UnormS8Uint,
	};
	vk::Format framebuffer_depth_format= vk::Format::eD16Unorm;
	for(const vk::Format depth_format_candidate : depth_formats)
	{
		const vk::FormatProperties format_properties=
			physical_device.getFormatProperties(depth_format_candidate);

		const vk::FormatFeatureFlags required_falgs= vk::FormatFeatureFlagBits::eDepthStencilAttachment;
		if((format_properties.optimalTilingFeatures & required_falgs) == required_falgs)
		{
			framebuffer_depth_format= depth_format_candidate;
			break;
		}
	}
	Log::Info("Framebuffer depth format: ", vk::to_string(framebuffer_depth_format));

	const vk::Format swapchain_surface_format= window_vulkan.GetSwapchainSurfaceFormat();

	const vk::AttachmentDescription attachment_descriptions[]
	{
		{
			vk::AttachmentDescriptionFlags(),
			swapchain_surface_format,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		},
		{
			vk::AttachmentDescriptionFlags(),
			framebuffer_depth_format,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal, // Actually we do not care about layout after this pass.
		},
	};

	const vk::AttachmentReference attachment_reference_color(0u, vk::ImageLayout::eColorAttachmentOptimal);
	const vk::AttachmentReference attachment_reference_depth(1u, vk::ImageLayout::eGeneral);

	const vk::SubpassDescription subpass_description(
		vk::SubpassDescriptionFlags(),
		vk::PipelineBindPoint::eGraphics,
		0u, nullptr,
		1u, &attachment_reference_color,
		nullptr,
		&attachment_reference_depth);

	render_pass_=
		vk_device_.createRenderPassUnique(
			vk::RenderPassCreateInfo(
				vk::RenderPassCreateFlags(),
				uint32_t(std::size(attachment_descriptions)), attachment_descriptions,
				1u, &subpass_description));

	const std::vector<vk::Image> swapchain_images= vk_device_.getSwapchainImagesKHR(window_vulkan.GetSwapchain());
	framebuffers_.resize(swapchain_images.size());
	for(size_t i= 0u; i < framebuffers_.size(); ++i)
	{
		{
			framebuffers_[i].depth_image=
				vk_device_.createImageUnique(
					vk::ImageCreateInfo(
						vk::ImageCreateFlags(),
						vk::ImageType::e2D,
						framebuffer_depth_format,
						vk::Extent3D(viewport_size_.width, viewport_size_.height, 1u),
						1u,
						1u,
						vk::SampleCountFlagBits::e1,
						vk::ImageTiling::eOptimal,
						vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
						vk::SharingMode::eExclusive,
						0u, nullptr,
						vk::ImageLayout::eUndefined));

			const vk::MemoryRequirements image_memory_requirements=
					vk_device_.getImageMemoryRequirements(*framebuffers_[i].depth_image);

			vk::MemoryAllocateInfo memory_allocate_info(image_memory_requirements.size);
			for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
			{
				if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
					(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				{
					memory_allocate_info.memoryTypeIndex= i;
					break;
				}
			}

			framebuffers_[i].depth_image_memory= vk_device_.allocateMemoryUnique(memory_allocate_info);
			vk_device_.bindImageMemory(*framebuffers_[i].depth_image, *framebuffers_[i].depth_image_memory, 0u);

			framebuffers_[i].depth_image_view=
				vk_device_.createImageViewUnique(
					vk::ImageViewCreateInfo(
						vk::ImageViewCreateFlags(),
						*framebuffers_[i].depth_image,
						vk::ImageViewType::e2D,
						framebuffer_depth_format,
						vk::ComponentMapping(),
						vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0u, 1u, 0u, 1u)));
		}

		framebuffers_[i].image= swapchain_images[i];

		framebuffers_[i].image_view=
			vk_device_.createImageViewUnique(
				vk::ImageViewCreateInfo(
					vk::ImageViewCreateFlags(),
					framebuffers_[i].image,
					vk::ImageViewType::e2D,
					swapchain_surface_format,
					vk::ComponentMapping(),
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u)));

		const vk::ImageView attachments[]{ *framebuffers_[i].image_view, *framebuffers_[i].depth_image_view };
		framebuffers_[i].framebuffer=
			vk_device_.createFramebufferUnique(
				vk::FramebufferCreateInfo(
					vk::FramebufferCreateFlags(),
					*render_pass_,
					uint32_t(std::size(attachments)), attachments,
					viewport_size_.width, viewport_size_.height, 1u));
	}
}

} // namespace HexGPU
