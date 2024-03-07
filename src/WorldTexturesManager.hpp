#pragma once
#include "TaskOrganiser.hpp"
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldTexturesManager
{
public:
	explicit WorldTexturesManager(WindowVulkan& window_vulkan);
	~WorldTexturesManager();

	void PrepareFrame(TaskOrganiser& task_organiser);

	vk::ImageView GetImageView() const;
	TaskOrganiser::ImageInfo GetImageInfo() const;

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const vk::UniqueImage image_;
	vk::UniqueImageView image_view_;
	vk::UniqueDeviceMemory image_memory_;

	vk::UniqueBuffer staging_buffer_;
	vk::UniqueDeviceMemory staging_buffer_memory_;

	bool textures_loaded_= false;
};

} // namespace HexGPU
