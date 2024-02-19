#pragma once
#include "HexGPUVulkan.hpp"

namespace HexGPU
{

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

} // namespace HexGPU
