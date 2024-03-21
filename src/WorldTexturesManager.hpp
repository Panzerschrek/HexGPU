#pragma once
#include "TaskOrganizer.hpp"
#include "TexturesTable.hpp"
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldTexturesManager
{
public:
	WorldTexturesManager(WindowVulkan& window_vulkan, vk::DescriptorPool global_descriptor_pool);
	~WorldTexturesManager();

	void PrepareFrame(TaskOrganizer& task_organizer);

	vk::ImageView GetImageView() const;
	vk::ImageView GetWaterImageView() const;
	TaskOrganizer::ImageInfo GetImageInfo() const;

private:
	static constexpr uint32_t c_num_layers= uint32_t(std::size(c_block_textures_table));

	struct TextureGenPipelineData
	{
		vk::UniqueShaderModule shader;
		vk::UniquePipeline pipeline;
		vk::UniqueImageView image_layer_view;
		vk::DescriptorSet descriptor_set;
	};

	struct TextureGenPipelines
	{
		vk::UniqueDescriptorSetLayout descriptor_set_layout;
		vk::UniquePipelineLayout pipeline_layout;
		std::array<TextureGenPipelineData, c_num_layers> pipelines;
	};

private:
	static TextureGenPipelines CreatePipelines(
		vk::Device vk_device,
		vk::DescriptorPool global_descriptor_pool,
		vk::Image image);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const vk::UniqueImage image_;
	const vk::UniqueDeviceMemory image_memory_;
	const vk::UniqueImageView image_view_;
	const vk::UniqueImageView water_image_view_;

	const TextureGenPipelines texture_gen_pipelines_;

	bool textures_generated_= false;
};

} // namespace HexGPU
