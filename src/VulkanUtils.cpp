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

} // namespace HexGPU
