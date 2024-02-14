#pragma once
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldTexturesManager
{
public:
	explicit WorldTexturesManager(WindowVulkan& window_vulkan);
	~WorldTexturesManager();

private:
	const vk::Device vk_device_;
};

} // namespace HexGPU
