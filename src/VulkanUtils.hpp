#pragma once
#include "HexGPUVulkan.hpp"

namespace HexGPU
{

using ShaderBindingIndex= uint32_t;

// Helper function to deal with different result signature of pipeline creation functions in different versions of Vulkan headers.
template<typename T>
vk::UniquePipeline UnwrapPipeline(T pipeline_create_result)
{
#ifdef VK_API_VERSION_1_3
	return std::get<1>(std::move(pipeline_create_result).asTuple());
#else
	return std::move(pipeline_create_result);
#endif
}

// Create compute pipeline with default "main" entry point.
vk::UniquePipeline CreateComputePipeline(
	vk::Device vk_device,
	vk::ShaderModule shader,
	vk::PipelineLayout pipeline_layout);

vk::UniqueDeviceMemory AllocateAndBindImageMemory(
	vk::Device vk_device,
	vk::Image image,
	const vk::PhysicalDeviceMemoryProperties& memory_properties);

} // namespace HexGPU
