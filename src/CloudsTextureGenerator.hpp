#pragma once
#include "TaskOrganizer.hpp"
#include "Pipeline.hpp"
#include "WindowVulkan.hpp"

namespace HexGPU
{

class CloudsTextureGenerator
{
public:
	CloudsTextureGenerator(WindowVulkan& window_vulkan, vk::DescriptorPool global_descriptor_pool);
	~CloudsTextureGenerator();

	void PrepareFrame(TaskOrganizer& task_organizer);

	vk::ImageView GetCloudsImageView() const;
	TaskOrganizer::ImageInfo GetImageInfo() const;

private:
	const vk::Device vk_device_;

	const ComputePipeline gen_pipeline_;
	const vk::DescriptorSet gen_descriptor_set_;

	const vk::UniqueImage image_;
	const vk::UniqueDeviceMemory image_memory_;
	const vk::UniqueImageView image_view_;

	bool generated_= false;
};

} // namespace HexGPU
