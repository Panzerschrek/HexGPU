#pragma once
#include "Pipeline.hpp"
#include "Buffer.hpp"
#include "GPUDataUploader.hpp"
#include "WorldGeometryGenerator.hpp"
#include "WorldTexturesGenerator.hpp"

namespace HexGPU
{

class WorldRenderer
{
public:
	WorldRenderer(
		WindowVulkan& window_vulkan,
		GPUDataUploader& gpu_data_uploader,
		const WorldProcessor& world_processor,
		vk::DescriptorPool global_descriptor_pool);

	~WorldRenderer();

	void PrepareFrame(TaskOrganizer& task_organizer);
	void CollectFrameInputs(TaskOrganizer::GraphicsTaskParams& out_task_params);
	void DrawOpaque(vk::CommandBuffer command_buffer);
	void DrawTransparent(vk::CommandBuffer command_buffer, float time_s);

private:
	struct WorldDrawPipeline : public GraphicsPipeline
	{
		vk::UniqueSampler texture_sampler;
	};

private:
	static WorldDrawPipeline CreateWorldDrawPipeline(
		vk::Device vk_device,
		vk::Extent2D viewport_size,
		vk::RenderPass render_pass);

	static WorldDrawPipeline CreateWorldWaterDrawPipeline(
		vk::Device vk_device,
		vk::Extent2D viewport_size,
		vk::RenderPass render_pass);

	static WorldDrawPipeline CreateFireDrawPipeline(
		vk::Device vk_device,
		vk::Extent2D viewport_size,
		vk::RenderPass render_pass);

	void CopyViewMatrix(TaskOrganizer& task_organizer);
	void BuildDrawIndirectBuffer(TaskOrganizer& task_organizer);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	const WorldProcessor& world_processor_;

	const WorldSizeChunks world_size_;

	WorldGeometryGenerator geometry_generator_;
	WorldTexturesGenerator textures_generator_;

	const Buffer draw_indirect_buffer_;
	const Buffer water_draw_indirect_buffer_;
	const Buffer uniform_buffer_;

	const ComputePipeline draw_indirect_buffer_build_pipeline_;
	const vk::DescriptorSet draw_indirect_buffer_build_descriptor_set_;

	const WorldDrawPipeline draw_pipeline_;
	const vk::DescriptorSet descriptor_set_;

	const WorldDrawPipeline water_draw_pipeline_;
	const vk::DescriptorSet water_descriptor_set_;

	const WorldDrawPipeline fire_draw_pipeline_;
	const vk::DescriptorSet fire_descriptor_set_;

	const Buffer index_buffer_;
};

} // namespace HexGPU
