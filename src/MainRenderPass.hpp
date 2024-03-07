#pragma once
#include "WindowVulkan.hpp"

namespace HexGPU
{

// Render pass for rendering of main geoemtry.
// For now renders directly to screen.
class MainRenderPass
{
public:
	explicit MainRenderPass(WindowVulkan& window_vulkan);

private:
	struct FramebufferData
	{
		vk::Image image;
		vk::UniqueImageView image_view;
		vk::UniqueFramebuffer framebuffer;

		vk::UniqueImage depth_image;
		vk::UniqueDeviceMemory depth_image_memory;
		vk::UniqueImageView depth_image_view;
	};

private:
	const vk::Device vk_device_;
	const vk::Extent2D viewport_size_;

	vk::UniqueRenderPass render_pass_;
	std::vector<FramebufferData> framebuffers_; // one framebuffer for each swapchain image.
};

} // namespace HexGPU
