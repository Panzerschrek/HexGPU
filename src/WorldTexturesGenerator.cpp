#include "WorldTexturesGenerator.hpp"
#include "GlobalDescriptorPool.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

namespace TextureGenShaderBindings
{
	const ShaderBindingIndex out_image= 0;
}

} // namespace

WorldTexturesGenerator::WorldTexturesGenerator(WindowVulkan& window_vulkan, const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, image_(vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::Extent3D(c_texture_size, c_texture_size, 1u),
			c_num_mips,
			c_num_layers,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			0u, nullptr,
			vk::ImageLayout::eUndefined)))
	, image_memory_(AllocateAndBindImageMemory(vk_device_, *image_, window_vulkan.GetMemoryProperties()))
	, image_view_(vk_device_.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			*image_,
			vk::ImageViewType::e2DArray,
			vk::Format::eR8G8B8A8Unorm,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, 0u, c_num_layers))))
	, water_image_view_(CreateLayerView(vk_device_, *image_, c_water_image_index))
	, fire_image_view_(CreateLayerView(vk_device_, *image_, c_fire_image_index))
	, grass_image_view_(CreateLayerView(vk_device_, *image_, c_grass_image_index))
	, texture_gen_pipelines_(CreatePipelines(vk_device_, global_descriptor_pool, *image_))
{
}

WorldTexturesGenerator::~WorldTexturesGenerator()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldTexturesGenerator::PrepareFrame(TaskOrganizer& task_organizer)
{
	if(textures_generated_)
		return;
	textures_generated_= true;

	TaskOrganizer::ComputeTaskParams task;
	task.output_images.push_back(GetImageInfo());

	const auto task_func=
		[this](const vk::CommandBuffer command_buffer)
		{
			for(size_t i= 0; i < c_num_layers; ++i)
			{
				command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *texture_gen_pipelines_.pipelines[i].pipeline);

				command_buffer.bindDescriptorSets(
					vk::PipelineBindPoint::eCompute,
					*texture_gen_pipelines_.pipeline_layout,
					0u,
					{texture_gen_pipelines_.pipelines[i].descriptor_set},
					{});

				// This constant should match workgroup size in shader!
				constexpr uint32_t c_workgroup_size[]{8, 16, 1};
				static_assert(c_texture_size % c_workgroup_size[0] == 0, "Wrong workgroup size!");
				static_assert(c_texture_size % c_workgroup_size[1] == 0, "Wrong workgroup size!");
				static_assert(c_workgroup_size[2] == 1, "Wrong workgroup size!");

				command_buffer.dispatch(c_texture_size / c_workgroup_size[0], c_texture_size / c_workgroup_size[1], 1);
			}
		};

	task_organizer.ExecuteTask(task, task_func);

	// Generate mips.
	task_organizer.GenerateImageMips(GetImageInfo(), vk::Extent2D(c_texture_size, c_texture_size));
}

vk::ImageView WorldTexturesGenerator::GetImageView() const
{
	return image_view_.get();
}

vk::ImageView WorldTexturesGenerator::GetWaterImageView() const
{
	return water_image_view_.get();
}

vk::ImageView WorldTexturesGenerator::GetFireImageView() const
{
	return fire_image_view_.get();
}

vk::ImageView WorldTexturesGenerator::GetGrassImageView() const
{
	return grass_image_view_.get();
}

TaskOrganizer::ImageInfo WorldTexturesGenerator::GetImageInfo() const
{
	TaskOrganizer::ImageInfo info;
	info.image= *image_;
	info.asppect_flags= vk::ImageAspectFlagBits::eColor;
	info.num_mips= c_num_mips;
	info.num_layers= c_num_layers;

	return info;
}

WorldTexturesGenerator::TextureGenPipelines WorldTexturesGenerator::CreatePipelines(
	const vk::Device vk_device,
	const vk::DescriptorPool global_descriptor_pool,
	const vk::Image image)
{
	TextureGenPipelines pipelines;

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			TextureGenShaderBindings::out_image,
			vk::DescriptorType::eStorageImage,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipelines.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	pipelines.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipelines.descriptor_set_layout,
			0u, nullptr));

	for(uint32_t i= 0; i < c_num_layers; ++i)
	{
		pipelines.pipelines[i].shader= CreateShader(vk_device, gen_shader_table[i]);
		pipelines.pipelines[i].pipeline=
			CreateComputePipeline(vk_device, *pipelines.pipelines[i].shader, *pipelines.pipeline_layout);

		pipelines.pipelines[i].descriptor_set= CreateDescriptorSet(
			vk_device,
			global_descriptor_pool,
			*pipelines.descriptor_set_layout);

		pipelines.pipelines[i].image_layer_view= vk_device.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			image,
			vk::ImageViewType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, i, 1u)));

		const vk::DescriptorImageInfo descriptor_tex_info(
			vk::Sampler(),
			*pipelines.pipelines[i].image_layer_view,
			vk::ImageLayout::eGeneral);

		vk_device.updateDescriptorSets(
			{
				{
					pipelines.pipelines[i].descriptor_set,
					TextureGenShaderBindings::out_image,
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

	return pipelines;
}

vk::UniqueImageView WorldTexturesGenerator::CreateLayerView(
	const vk::Device vk_device,
	const vk::Image image,
	const uint32_t layer_index)
{
	return vk_device.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			image,
			vk::ImageViewType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, layer_index, 1u)));
}

} // namespace HexGPU
