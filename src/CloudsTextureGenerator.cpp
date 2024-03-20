#include "CloudsTextureGenerator.hpp"
#include "GlobalDescriptorPool.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

const uint32_t c_texture_size_log2= 7;
const uint32_t c_texture_size= 1 << c_texture_size_log2;

const uint32_t c_num_mips= 1; // TODO - make mips

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

vk::UniqueDeviceMemory AllocateAndBindImageMemory(const vk::Image image, const WindowVulkan& window_vulkan)
{
	const auto vk_device= window_vulkan.GetVulkanDevice();

	const auto memory_properties= window_vulkan.GetMemoryProperties();

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

} // namespace

CloudsTextureGenerator::CloudsTextureGenerator(WindowVulkan& window_vulkan, const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
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
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage,
			vk::SharingMode::eExclusive,
			0u, nullptr,
			vk::ImageLayout::eUndefined)))
	, image_memory_(AllocateAndBindImageMemory(*image_, window_vulkan))
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
