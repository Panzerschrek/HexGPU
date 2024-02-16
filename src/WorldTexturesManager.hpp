#pragma once
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldTexturesManager
{
public:
	explicit WorldTexturesManager(WindowVulkan& window_vulkan);
	~WorldTexturesManager();

	void PrepareFrame(vk::CommandBuffer command_buffer);

	vk::ImageView GetImageView() const;

private:
	const vk::Device vk_device_;
	const uint32_t vk_queue_family_index_;
	const vk::PhysicalDeviceMemoryProperties memory_properties_;

	vk::UniqueImage image_;
	vk::UniqueImageView image_view_;
	vk::UniqueDeviceMemory image_memory_;

	vk::UniqueBuffer staging_buffer_;
	vk::UniqueDeviceMemory staging_buffer_memory_;

	bool textures_loaded_= false;
};

} // namespace HexGPU
