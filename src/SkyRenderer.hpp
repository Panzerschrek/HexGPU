#pragma once
#include "CloudsTextureGenerator.hpp"
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
	void Draw(vk::CommandBuffer command_buffer, float time_s);

private:
	struct CloudsPipeline : public GraphicsPipeline
	{
		vk::UniqueSampler texture_sampler;
	};

private:
	void DrawSkybox(vk::CommandBuffer command_buffer);
	void DrawClouds(vk::CommandBuffer command_buffer, float time_s);

	static CloudsPipeline CreateCloudsPipeline(
		vk::Device vk_device,
		vk::Extent2D viewport_size,
		vk::RenderPass render_pass);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	const WorldProcessor& world_processor_;

	CloudsTextureGenerator clouds_texture_generator_;

	const Buffer uniform_buffer_;

	const GraphicsPipeline skybox_pipeline_;
	const vk::DescriptorSet skybox_descriptor_set_;

	const CloudsPipeline clouds_pipeline_;
	const vk::DescriptorSet clouds_descriptor_set_;
};

} // namespace HexGPU
