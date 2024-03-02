#include "HexGPUVulkan.hpp"

namespace HexGPU
{

// Creates descriptor pool situable for allocating descriptors which are created once and live the whole life of the applocation.
// It contains large enough number of descriptors.
// Individual sets should not be freed, so, use "vk::DescriptorSet" to store them.
vk::UniqueDescriptorPool CreateGlobalDescriptorPool(vk::Device vk_device);

// Helper function for single descriptor set creation.
// Result is non-owning handle.
vk::DescriptorSet CreateDescriptorSet(
	vk::Device vk_device,
	vk::DescriptorPool descriptor_pool,
	vk::DescriptorSetLayout descriptor_set_layout);

} // namespace HexGPU
