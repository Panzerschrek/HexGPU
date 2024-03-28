#pragma once
#include "Pipeline.hpp"
#include "WorldProcessor.hpp"
#include "WorldRenderPass.hpp"

namespace HexGPU
{

class BuildPrismRenderer
{
public:
	BuildPrismRenderer(
		WindowVulkan& window_vulkan,
		WorldRenderPass& world_render_pass,
		const WorldProcessor& world_processor,
		vk::DescriptorPool global_descriptor_pool);

	~BuildPrismRenderer();

	void PrepareFrame(TaskOrganizer& task_organizer);
	void CollectFrameInputs(TaskOrganizer::GraphicsTaskParams& out_task_params);
	void Draw(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	const WorldProcessor& world_processor_;

	const Buffer uniform_buffer_;

	const GraphicsPipeline pipeline_;
	const vk::DescriptorSet descriptor_set_;
};

} // namespace HexGPU
