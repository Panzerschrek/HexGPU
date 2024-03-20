#pragma once
#include "TaskOrganizer.hpp"
#include "Pipeline.hpp"
#include "WindowVulkan.hpp"

namespace HexGPU
{

class TexturesGenerator
{
public:
	TexturesGenerator(WindowVulkan& window_vulkan, vk::DescriptorPool global_descriptor_pool);
	~TexturesGenerator();

	void PrepareFrame(TaskOrganizer& task_organizer);

	vk::ImageView GetCloudsImageView() const;
	TaskOrganizer::ImageInfo GetCloudsImageInfo() const;

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const ComputePipeline clouds_texture_gen_pipeline_;
	const vk::DescriptorSet clouds_texture_gen_descriptor_set_;

	const vk::UniqueImage image_;
	vk::UniqueImageView image_view_;
	vk::UniqueDeviceMemory image_memory_;

	bool textures_generated_= false;
};

} // namespace HexGPU
