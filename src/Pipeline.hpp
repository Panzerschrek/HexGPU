#pragma once
#include "HexGPUVulkan.hpp"

namespace HexGPU
{

// A struct combining common entities for a Vulkan pipeline.
struct Pipeline
{
	// These Vulkan objects are created almost always together. So, group them.
	vk::UniqueDescriptorSetLayout descriptor_set_layout;
	vk::UniquePipelineLayout pipeline_layout;
	vk::UniquePipeline pipeline;
};

struct ComputePipeline : public Pipeline
{
	vk::UniqueShaderModule shader;
};

struct GraphicsPipeline : public Pipeline
{
	vk::UniqueShaderModule shader_vert;
	vk::UniqueShaderModule shader_frag;
};

} // namespace HexGPU
