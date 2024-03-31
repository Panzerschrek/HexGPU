#pragma once
#include "Pipeline.hpp"
#include "Buffer.hpp"
#include "GPUDataUploader.hpp"
#include "WorldGeometryGenerator.hpp"
#include "WorldRenderPass.hpp"
#include "WorldTexturesGenerator.hpp"

namespace HexGPU
{

class WorldRenderer
{
public:
	WorldRenderer(
		WindowVulkan& window_vulkan,
		WorldRenderPass& world_render_pass,
		GPUDataUploader& gpu_data_uploader,
		const WorldProcessor& world_processor,
		vk::DescriptorPool global_descriptor_pool);

	~WorldRenderer();

	void PrepareFrame(TaskOrganizer& task_organizer);
	void CollectFrameInputs(TaskOrganizer::GraphicsTaskParams& out_task_params);
	void DrawOpaque(vk::CommandBuffer command_buffer, float time_s);
	void DrawTransparent(vk::CommandBuffer command_buffer, float time_s);

private:
	void DrawWorld(vk::CommandBuffer command_buffer);
	void DrawWater(vk::CommandBuffer command_buffer, float time_s);
	void DrawFire(vk::CommandBuffer command_buffer, float time_s);
	void DrawGrass(vk::CommandBuffer command_buffer);

	static GraphicsPipeline CreateWorldDrawPipeline(
		vk::Device vk_device,
		bool use_supersampling,
		vk::SampleCountFlagBits samples,
		vk::Extent2D viewport_size,
		vk::RenderPass render_pass,
		vk::Sampler texture_sampler);

	static GraphicsPipeline CreateWorldWaterDrawPipeline(
		vk::Device vk_device,
		vk::SampleCountFlagBits samples,
		vk::Extent2D viewport_size,
		vk::RenderPass render_pass,
		vk::Sampler texture_sampler);

	static GraphicsPipeline CreateFireDrawPipeline(
		vk::Device vk_device,
		bool use_supersampling,
		vk::SampleCountFlagBits samples,
		vk::Extent2D viewport_size,
		vk::RenderPass render_pass,
		vk::Sampler texture_sampler);

	static GraphicsPipeline CreateGrassDrawPipeline(
		vk::Device vk_device,
		bool use_supersampling,
		vk::SampleCountFlagBits samples,
		vk::Extent2D viewport_size,
		vk::RenderPass render_pass,
		vk::Sampler texture_sampler);

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
	const Buffer fire_draw_indirect_buffer_;
	const Buffer grass_draw_indirect_buffer_;
	const Buffer uniform_buffer_;

	const ComputePipeline draw_indirect_buffer_build_pipeline_;
	const vk::DescriptorSet draw_indirect_buffer_build_descriptor_set_;

	const vk::UniqueSampler texture_sampler_;

	const GraphicsPipeline draw_pipeline_;
	const vk::DescriptorSet descriptor_set_;

	const GraphicsPipeline water_draw_pipeline_;
	const vk::DescriptorSet water_descriptor_set_;

	const GraphicsPipeline fire_draw_pipeline_;
	const vk::DescriptorSet fire_descriptor_set_;

	const GraphicsPipeline grass_draw_pipeline_;
	const vk::DescriptorSet grass_descriptor_set_;

	const Buffer index_buffer_;
};

} // namespace HexGPU
