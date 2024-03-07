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

	void PrepareFrame(TaskOrganiser& task_organiser);
	void CollectFrameInputs(TaskOrganiser::GraphicsTaskParams& out_task_params);
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
