#pragma once
#include "SystemWindow.hpp"
#include "HexGPUVulkan.hpp"
#include <functional>

namespace HexGPU
{

class WindowVulkan final
{
public:
	using DrawFunction= std::function<void(vk::Framebuffer framebuffer)>;

public:
	WindowVulkan(const SystemWindow& system_window, Settings& settings);
	~WindowVulkan();

	vk::CommandBuffer BeginFrame();
	void EndFrame(const DrawFunction& draw_function);

	vk::Instance GetVulkanInstance() const;
	vk::PhysicalDevice GetPhysicalDevice() const;
	vk::Device GetVulkanDevice() const;
	vk::Extent2D GetViewportSize() const;
	uint32_t GetQueueFamilyIndex() const;
	vk::Queue GetQueue() const;
	vk::CommandPool GetCommandPool() const;
	vk::RenderPass GetRenderPass() const; // Render pass for rendering directly into screen.
	vk::PhysicalDeviceMemoryProperties GetMemoryProperties() const;

	// Command buffers are circulary reused.
	// When a new command buffer is started, its previous contents is guaranteed to be flushed.
	// So, it's safe to read on CPU data in frame #N, written in frame #N - #NumCommandBuffers.
	size_t GetNumCommandBuffers() const;

	uint32_t GetFramebufferImageCount() const;

private:
	struct CommandBufferData
	{
		vk::UniqueCommandBuffer command_buffer;
		vk::UniqueSemaphore image_available_semaphore;
		vk::UniqueSemaphore rendering_finished_semaphore;
		vk::UniqueFence submit_fence;
	};

	struct SwapchainFramebufferData
	{
		vk::Image image;
		vk::UniqueImageView image_view;
		vk::UniqueFramebuffer framebuffer;
	};

private:
	// Keep here order of construction.
	vk::UniqueInstance instance_;
	VkDebugReportCallbackEXT debug_report_callback_= VK_NULL_HANDLE;
	vk::UniqueSurfaceKHR surface_;
	vk::UniqueDevice vk_device_;
	vk::Queue queue_= nullptr;
	uint32_t queue_family_index_= ~0u;
	vk::Extent2D viewport_size_;
	vk::PhysicalDeviceMemoryProperties memory_properties_;
	vk::PhysicalDevice physical_device_;
	vk::UniqueSwapchainKHR swapchain_;

	vk::UniqueRenderPass render_pass_;
	std::vector<SwapchainFramebufferData> framebuffers_; // one framebuffer for each swapchain image.

	vk::UniqueCommandPool command_pool_;

	std::vector<CommandBufferData> command_buffers_;
	const CommandBufferData* current_frame_command_buffer_= nullptr;
	size_t frame_count_= 0u;
};

} // namespace HexGPU
