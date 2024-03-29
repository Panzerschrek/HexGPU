#pragma once
#include "WindowVulkan.hpp"
#include "Pipeline.hpp"
#include "Settings.hpp"
#include "TaskOrganizer.hpp"

namespace HexGPU
{

class WorldRenderPass
{
public:
	WorldRenderPass(WindowVulkan& window_vulkan, Settings& settings, vk::DescriptorPool global_descriptor_pool);
	~WorldRenderPass();

	vk::SampleCountFlagBits GetSamples() const;
	vk::Framebuffer GetFramebuffer() const;
	vk::Extent2D GetFramebufferSize() const;
	vk::RenderPass GetRenderPass() const;

	void CollectPassOutputs(TaskOrganizer::GraphicsTaskParams& out_task_params) const;
	void CollectFrameInputs(TaskOrganizer::GraphicsTaskParams& out_task_params) const;

	void Draw(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;

	const bool use_supersampling_;
	const vk::SampleCountFlagBits samples_;
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

	const vk::UniqueSampler sampler_;

	const GraphicsPipeline pipeline_;
	const vk::DescriptorSet descriptor_set_;
};

} // namespace HexGPU
