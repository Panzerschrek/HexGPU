#pragma once
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldRenderPass
{
public:
	explicit WorldRenderPass(WindowVulkan& window_vulkan);
	~WorldRenderPass();

private:
	const vk::Device vk_device_;

	const vk::Extent3D framebuffer_size_;

	const vk::UniqueImage image_;
	const vk::UniqueDeviceMemory image_memory_;
	const vk::UniqueImageView image_view_;

	const vk::Format depth_format_;
	const vk::UniqueImage depth_image_;
	const vk::UniqueDeviceMemory depth_image_memory_;
	const vk::UniqueImageView depth_image_view_;

	const vk::UniqueRenderPass render_pass_;

	const vk::UniqueFramebuffer framebuffer_;
};

} // namespace HexGPU
