#include "VulkanUtils.hpp"

namespace HexGPU
{

vk::UniquePipeline CreateComputePipeline(
	const vk::Device vk_device,
	const vk::ShaderModule shader,
	const vk::PipelineLayout pipeline_layout)
{
	return UnwrapPipeline(vk_device.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				shader,
				"main"),
			pipeline_layout)));
}

vk::UniqueDeviceMemory AllocateAndBindImageMemory(
	const vk::Device vk_device,
	const vk::Image image,
	const vk::PhysicalDeviceMemoryProperties& memory_properties)
{
	const vk::MemoryRequirements memory_requirements= vk_device.getImageMemoryRequirements(image);

	vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
	for(uint32_t j= 0u; j < memory_properties.memoryTypeCount; ++j)
	{
		if((memory_requirements.memoryTypeBits & (1u << j)) != 0 &&
			(memory_properties.memoryTypes[j].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
		{
			memory_allocate_info.memoryTypeIndex= j;
			break;
		}
	}

	auto image_memory= vk_device.allocateMemoryUnique(memory_allocate_info);
	vk_device.bindImageMemory(image, *image_memory, 0u);

	return image_memory;
}

} // namespace HexGPU
