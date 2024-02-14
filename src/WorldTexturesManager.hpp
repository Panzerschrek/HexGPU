#pragma once
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldTexturesManager
{
public:
	explicit WorldTexturesManager(WindowVulkan& window_vulkan);
	~WorldTexturesManager();

	vk::ImageView GetImageView() const;

private:
	const vk::Device vk_device_;

	vk::UniqueImage image_;
	vk::UniqueImageView image_view_;
	vk::UniqueDeviceMemory image_memory_;
};

} // namespace HexGPU
