#pragma once
#include "WorldProcessor.hpp"

namespace HexGPU
{

class SkyRenderer
{
public:
	SkyRenderer(
		WindowVulkan& window_vulkan,
		const WorldProcessor& world_processor,
		vk::DescriptorPool global_descriptor_pool);
	~SkyRenderer();

	void PrepareFrame(TaskOrganizer& task_organizer);
	void CollectFrameInputs(TaskOrganizer::GraphicsTaskParams& out_task_params);
	void Draw(vk::CommandBuffer command_buffer);

private:
	void DrawSkybox(vk::CommandBuffer command_buffer);
	void DrawClouds(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	const WorldProcessor& world_processor_;

	const Buffer uniform_buffer_;

	const GraphicsPipeline skybox_pipeline_;
	const vk::DescriptorSet skybox_descriptor_set_;

	const GraphicsPipeline clouds_pipeline_;
	const vk::DescriptorSet clouds_descriptor_set_;
};

} // namespace HexGPU
