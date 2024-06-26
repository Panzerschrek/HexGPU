#include "CloudsTextureGenerator.hpp"
#include "GlobalDescriptorPool.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

const uint32_t c_texture_size_log2= 8; // This must match size in generation shader!
const uint32_t c_texture_size= 1 << c_texture_size_log2;

const uint32_t c_num_mips= c_texture_size_log2 - 2;

namespace CloudsTextureGenPipelineBindings
{
	const ShaderBindingIndex out_image= 0;
}

ComputePipeline CreateCloudsTextureGenPipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::clouds_texture_gen_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			CloudsTextureGenPipelineBindings::out_image,
			vk::DescriptorType::eStorageImage,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			0u, nullptr));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

} // namespace

CloudsTextureGenerator::CloudsTextureGenerator(WindowVulkan& window_vulkan, const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, gen_pipeline_(CreateCloudsTextureGenPipeline(vk_device_))
	, gen_descriptor_set_(
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *gen_pipeline_.descriptor_set_layout))
	, image_(vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			vk::Format::eR8Unorm,
			vk::Extent3D(c_texture_size, c_texture_size, 1u),
			c_num_mips,
			1u,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage,
			vk::SharingMode::eExclusive,
			0u, nullptr,
			vk::ImageLayout::eUndefined)))
	, image_memory_(AllocateAndBindImageMemory(vk_device_, *image_, window_vulkan.GetMemoryProperties()))
	, image_view_(vk_device_.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			*image_,
			vk::ImageViewType::e2D,
			vk::Format::eR8Unorm,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, 0u, 1u))))
{
	// Update clouds texture gen descriptor set.
	{
		const vk::DescriptorImageInfo descriptor_tex_info(
			vk::Sampler(),
			*image_view_,
			vk::ImageLayout::eGeneral);

		vk_device_.updateDescriptorSets(
			{
				{
					gen_descriptor_set_,
					CloudsTextureGenPipelineBindings::out_image,
					0u,
					1u,
					vk::DescriptorType::eStorageImage,
					&descriptor_tex_info,
					nullptr,
					nullptr
				},
			},
			{});
	}
}

CloudsTextureGenerator::~CloudsTextureGenerator()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void CloudsTextureGenerator::PrepareFrame(TaskOrganizer& task_organizer)
{
	// Perform texture generation if not done this yet.
	if(generated_)
		return;
	generated_= true;

	TaskOrganizer::ComputeTaskParams task;
	task.output_images.push_back(GetImageInfo());

	const auto task_func=
		[this](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *gen_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*gen_pipeline_.pipeline_layout,
				0u,
				{gen_descriptor_set_},
				{});

			// This constant should match workgroup size in shader!
			constexpr uint32_t c_workgroup_size[]{8, 16, 1};
			static_assert(c_texture_size % c_workgroup_size[0] == 0, "Wrong workgroup size!");
			static_assert(c_texture_size % c_workgroup_size[1] == 0, "Wrong workgroup size!");
			static_assert(c_workgroup_size[2] == 1, "Wrong workgroup size!");

			command_buffer.dispatch(c_texture_size / c_workgroup_size[0], c_texture_size / c_workgroup_size[1], 1);
		};

	task_organizer.ExecuteTask(task, task_func);

	task_organizer.GenerateImageMips(GetImageInfo(), vk::Extent2D(c_texture_size, c_texture_size));
}

vk::ImageView CloudsTextureGenerator::GetCloudsImageView() const
{
	return image_view_.get();
}

TaskOrganizer::ImageInfo CloudsTextureGenerator::GetImageInfo() const
{
	TaskOrganizer::ImageInfo info;
	info.image= *image_;
	info.asppect_flags= vk::ImageAspectFlagBits::eColor;
	info.num_mips= c_num_mips;
	info.num_layers= 1;

	return info;
}

} // namespace HexGPU
