#include "TexturesGenerator.hpp"
#include "GlobalDescriptorPool.hpp"
#include "Image.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

const uint32_t c_texture_size_log2= 8;
const uint32_t c_texture_size= 1 << c_texture_size_log2;

const uint32_t c_num_mips= c_texture_size_log2 - 2; // Ignore last two mips for simplicity.

// Add extra padding (use 3/2 instead of 4/3).
const uint32_t c_texture_num_texels_with_mips= c_texture_size * c_texture_size * 3 / 2;

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

TexturesGenerator::TexturesGenerator(WindowVulkan& window_vulkan, const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, clouds_texture_gen_pipeline_(CreateCloudsTextureGenPipeline(vk_device_))
	, clouds_texture_gen_descriptor_set_(
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *clouds_texture_gen_pipeline_.descriptor_set_layout))
	, image_(vk_device_.createImageUnique(
		vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::Extent3D(c_texture_size, c_texture_size, 1u),
			c_num_mips,
			1u,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage,
			vk::SharingMode::eExclusive,
			0u, nullptr,
			vk::ImageLayout::eUndefined)))
{
	const auto memory_properties= window_vulkan.GetMemoryProperties();

	// Allocate image memory.
	{
		const vk::MemoryRequirements memory_requirements= vk_device_.getImageMemoryRequirements(*image_);

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

		image_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindImageMemory(*image_, *image_memory_, 0u);
	}

	// Create image view.
	image_view_= vk_device_.createImageViewUnique(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(),
			*image_,
			vk::ImageViewType::e2D,
			vk::Format::eR8G8B8A8Unorm,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, c_num_mips, 0u, 1u)));

	// Create staging buffer.
	// For now create it with size of whole texture (with mips and extra padding).
	const uint32_t buffer_data_size= c_texture_num_texels_with_mips * sizeof(PixelType) * 2;
	{
		staging_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					buffer_data_size,
					vk::BufferUsageFlagBits::eTransferSrc));

		const vk::MemoryRequirements memory_requirements= vk_device_.getBufferMemoryRequirements(*staging_buffer_);

		vk::MemoryAllocateInfo memory_allocate_info(memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible ) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= i;
				break;
			}
		}

		staging_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*staging_buffer_, *staging_buffer_memory_, 0u);
	}

	// Load files.
	// For now just fill data into the mapped staging buffer.

	void* staging_buffer_mapped= nullptr;
	vk_device_.mapMemory(*staging_buffer_memory_, 0u, buffer_data_size, vk::MemoryMapFlags(), &staging_buffer_mapped);


	PixelType* dst= static_cast<PixelType*>(staging_buffer_mapped);
	LoadImageWithExpectedSize((std::string("textures/") + "leaves3.png").c_str(), c_texture_size, c_texture_size, dst);

	for(uint32_t mip= 1; mip < c_num_mips; ++mip)
	{
		const uint32_t current_size= c_texture_size >> mip;
		const uint32_t prev_size= current_size << 1;

		PixelType* const mip_dst= dst + prev_size * prev_size;
		RGBA8_GetMip(reinterpret_cast<const uint8_t*>(dst), reinterpret_cast<uint8_t*>(mip_dst), prev_size, prev_size);

		dst= mip_dst;
	}

	vk_device_.unmapMemory(*staging_buffer_memory_);

	// Update clouds texture gen descriptor set.
	{
		const vk::DescriptorImageInfo descriptor_tex_info(
			vk::Sampler(),
			*image_view_,
			vk::ImageLayout::eGeneral);

		vk_device_.updateDescriptorSets(
			{
				{
					clouds_texture_gen_descriptor_set_,
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

TexturesGenerator::~TexturesGenerator()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void TexturesGenerator::PrepareFrame(TaskOrganizer& task_organizer)
{
	// Copy buffer into the image after ininitialization, because we need a command buffer.

	if(textures_loaded_)
		return;
	textures_loaded_= true;

	TaskOrganizer::TransferTaskParams task;
	task.input_buffers.push_back(*staging_buffer_);
	task.output_images.push_back(GetCloudsImageInfo());

	const auto task_func=
		[this](const vk::CommandBuffer command_buffer)
		{
		/*
			uint32_t offset= 0u;

			for(uint32_t mip= 0; mip < c_num_mips; ++mip)
			{
				const uint32_t current_size= c_texture_size >> mip;

				command_buffer.copyBufferToImage(
					*staging_buffer_,
					*image_,
					vk::ImageLayout::eTransferDstOptimal,
					{
						{
							offset,
							current_size, current_size,
							vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, mip, 0, 1u),
							vk::Offset3D(0, 0, 0),
							vk::Extent3D(current_size, current_size, 1u)
						}
					});

				offset+= current_size * current_size * uint32_t(sizeof(PixelType));
			}*/

			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *clouds_texture_gen_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*clouds_texture_gen_pipeline_.pipeline_layout,
				0u,
				{clouds_texture_gen_descriptor_set_},
				{});

			command_buffer.dispatch(c_texture_size, c_texture_size, 1);
		};

	task_organizer.ExecuteTask(task, task_func);
}

vk::ImageView TexturesGenerator::GetCloudsImageView() const
{
	return image_view_.get();
}

TaskOrganizer::ImageInfo TexturesGenerator::GetCloudsImageInfo() const
{
	TaskOrganizer::ImageInfo info;
	info.image= *image_;
	info.asppect_flags= vk::ImageAspectFlagBits::eColor;
	info.num_mips= c_num_mips;
	info.num_layers= 1;

	return info;
}

} // namespace HexGPU
