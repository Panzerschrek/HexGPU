#include "GlobalDescriptorPool.hpp"

namespace HexGPU
{

// Creates descriptor pool situable for allocating descriptors which are created once and live the whole life of the applocation.
vk::UniqueDescriptorPool CreateGlobalDescriptorPool(const vk::Device vk_device)
{
	const uint32_t max_sets= 512u;

	// Add here other sizes if necessary.
	const vk::DescriptorPoolSize descriptor_pool_sizes[]
	{
		{vk::DescriptorType::eStorageBuffer, 256u},
		{vk::DescriptorType::eUniformBuffer, 128u},
		{vk::DescriptorType::eCombinedImageSampler, 64u},
	};

	return
		vk_device.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits(), // Do not allow freeing individual sets.
				max_sets,
				uint32_t(std::size(descriptor_pool_sizes)), descriptor_pool_sizes));
}

} // namespace HexGPU
