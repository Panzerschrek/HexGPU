#include "WorldRenderPass.hpp"
#include "Log.hpp"
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

vk::UniqueRenderPass CreateRenderPass(const vk::Device vk_device, const vk::Format depth_format)
{
	const vk::AttachmentDescription attachment_descriptions[]
	{
		{
			vk::AttachmentDescriptionFlags(),
			vk::Format::eR8G8B8A8Unorm,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR // TODO - use other layout
		},
		{
			vk::AttachmentDescriptionFlags(),
			depth_format,
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

} // namespace

WorldRenderPass::WorldRenderPass(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, framebuffer_size_(GetFramebufferTextureSize(window_vulkan))
	, image_(vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			framebuffer_size_,
			1u,
			1u,
			vk::SampleCountFlagBits::e1,
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
			vk::SampleCountFlagBits::e1,
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
	, render_pass_(CreateRenderPass(vk_device_, depth_format_))
	, framebuffer_(CreateFramebuffer(vk_device_, *image_view_, *depth_image_view_, *render_pass_, framebuffer_size_))
{
}

WorldRenderPass::~WorldRenderPass()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

} // namespace HexGPU
