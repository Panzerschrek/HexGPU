#include "TaskOrganizer.hpp"
#include "Assert.hpp"

namespace HexGPU
{

TaskOrganizer::TaskOrganizer(WindowVulkan& window_vulkan)
	: queue_family_index_(window_vulkan.GetQueueFamilyIndex())
{
}

void TaskOrganizer::SetCommandBuffer(vk::CommandBuffer command_buffer)
{
	command_buffer_= command_buffer;
}

void TaskOrganizer::ExecuteTask(const ComputeTaskParams& params, const TaskFunc& func)
{
	std::vector<vk::BufferMemoryBarrier> buffer_barriers;
	std::vector<vk::ImageMemoryBarrier> image_barriers;
	vk::PipelineStageFlags src_pipeline_stage_flags;
	vk::PipelineStageFlags dst_pipeline_stage_flags;
	bool require_barrier_for_write_after_read_sync= false;

	if(const auto dst_sync_info= GetBufferDstSyncInfo(BufferUsage::ComputeShaderSrc))
	{
		for(const vk::Buffer buffer : params.input_storage_buffers)
		{
			if(const auto src_sync_info= GetBufferSrcSyncInfoForLastUsage(buffer))
			{
				buffer_barriers.emplace_back(
					src_sync_info->access_flags, dst_sync_info->access_flags,
					queue_family_index_, queue_family_index_,
					buffer,
					0, VK_WHOLE_SIZE);
				src_pipeline_stage_flags|= src_sync_info->pipeline_stage_flags;
				dst_pipeline_stage_flags|= dst_sync_info->pipeline_stage_flags;
			}
		}

		for(const vk::Buffer buffer : params.input_output_storage_buffers)
		{
			if(const auto src_sync_info= GetBufferSrcSyncInfoForLastUsage(buffer))
			{
				buffer_barriers.emplace_back(
					src_sync_info->access_flags, dst_sync_info->access_flags,
					queue_family_index_, queue_family_index_,
					buffer,
					0, VK_WHOLE_SIZE);
				src_pipeline_stage_flags|= src_sync_info->pipeline_stage_flags;
				dst_pipeline_stage_flags|= dst_sync_info->pipeline_stage_flags;
			}
			if(const auto last_usage= GetLastBufferUsage(buffer))
			{
				if(IsReadBufferUsage(*last_usage))
				{
					require_barrier_for_write_after_read_sync= true;
					src_pipeline_stage_flags|= GetPipelineStageForBufferUsage(*last_usage);
					dst_pipeline_stage_flags|= vk::PipelineStageFlagBits::eComputeShader;
				}
			}
		}
	}

	for(const vk::Buffer buffer : params.output_storage_buffers)
	{
		if(const auto last_usage= GetLastBufferUsage(buffer))
		{
			if(IsReadBufferUsage(*last_usage))
			{
				require_barrier_for_write_after_read_sync= true;
				src_pipeline_stage_flags|= GetPipelineStageForBufferUsage(*last_usage);
				dst_pipeline_stage_flags|= vk::PipelineStageFlagBits::eComputeShader;
			}
		}
	}

	for(const ImageInfo& image_info : params.output_images)
	{
		if(GetLastImageUsage(image_info.image) != ImageUsage::ComputeDst)
		{
			const auto sync_info= GetSyncInfoForLastImageUsage(image_info.image);
			image_barriers.emplace_back(
				sync_info.access_flags, vk::AccessFlagBits::eShaderWrite,
				sync_info.layout, vk::ImageLayout::eGeneral,
				queue_family_index_, queue_family_index_,
				image_info.image,
				vk::ImageSubresourceRange(image_info.asppect_flags, 0u, image_info.num_mips, 0u, image_info.num_layers));

			src_pipeline_stage_flags|= sync_info.pipeline_stage_flags;
			dst_pipeline_stage_flags|= vk::PipelineStageFlagBits::eComputeShader;
		}
	}

	if(!buffer_barriers.empty() || !image_barriers.empty() || require_barrier_for_write_after_read_sync)
		command_buffer_.pipelineBarrier(
			src_pipeline_stage_flags,
			dst_pipeline_stage_flags,
			vk::DependencyFlags(),
			{},
			buffer_barriers,
			image_barriers);

	func(command_buffer_);

	UpdateLastBuffersUsage(params.input_storage_buffers, BufferUsage::ComputeShaderSrc);
	UpdateLastBuffersUsage(params.output_storage_buffers, BufferUsage::ComputeShaderDst);
	UpdateLastBuffersUsage(params.input_output_storage_buffers, BufferUsage::ComputeShaderDst);

	for(const ImageInfo& image_info : params.output_images)
		UpdateLastImageUsage(image_info.image, ImageUsage::ComputeDst);
}

void TaskOrganizer::ExecuteTask(const GraphicsTaskParams& params, const TaskFunc& func)
{
	std::vector<vk::BufferMemoryBarrier> buffer_barriers;
	std::vector<vk::ImageMemoryBarrier> image_barriers;
	vk::PipelineStageFlags src_pipeline_stage_flags;
	vk::PipelineStageFlags dst_pipeline_stage_flags;

	if(const auto dst_sync_info= GetBufferDstSyncInfo(BufferUsage::IndirectDrawSrc))
	{
		for(const vk::Buffer buffer : params.indirect_draw_buffers)
		{
			if(const auto src_sync_info= GetBufferSrcSyncInfoForLastUsage(buffer))
			{
				buffer_barriers.emplace_back(
					src_sync_info->access_flags, dst_sync_info->access_flags,
					queue_family_index_, queue_family_index_,
					buffer,
					0, VK_WHOLE_SIZE);
				src_pipeline_stage_flags|= src_sync_info->pipeline_stage_flags;
				dst_pipeline_stage_flags|= dst_sync_info->pipeline_stage_flags;
			}
		}
	}

	if(const auto dst_sync_info= GetBufferDstSyncInfo(BufferUsage::IndexSrc))
	{
		for(const vk::Buffer buffer : params.index_buffers)
		{
			if(const auto src_sync_info= GetBufferSrcSyncInfoForLastUsage(buffer))
			{
				buffer_barriers.emplace_back(
					src_sync_info->access_flags, dst_sync_info->access_flags,
					queue_family_index_, queue_family_index_,
					buffer,
					0, VK_WHOLE_SIZE);
				src_pipeline_stage_flags|= src_sync_info->pipeline_stage_flags;
				dst_pipeline_stage_flags|= dst_sync_info->pipeline_stage_flags;
			}
		}
	}

	if(const auto dst_sync_info= GetBufferDstSyncInfo(BufferUsage::VertexSrc))
	{
		for(const vk::Buffer buffer : params.vertex_buffers)
		{
			if(const auto src_sync_info= GetBufferSrcSyncInfoForLastUsage(buffer))
			{
				buffer_barriers.emplace_back(
					src_sync_info->access_flags, dst_sync_info->access_flags,
					queue_family_index_, queue_family_index_,
					buffer,
					0, VK_WHOLE_SIZE);
				src_pipeline_stage_flags|= src_sync_info->pipeline_stage_flags;
				dst_pipeline_stage_flags|= dst_sync_info->pipeline_stage_flags;
			}
		}
	}

	if(const auto dst_sync_info= GetBufferDstSyncInfo(BufferUsage::UniformSrc))
	{
		for(const vk::Buffer buffer : params.uniform_buffers)
		{
			if(const auto src_sync_info= GetBufferSrcSyncInfoForLastUsage(buffer))
			{
				buffer_barriers.emplace_back(
					src_sync_info->access_flags, dst_sync_info->access_flags,
					queue_family_index_, queue_family_index_,
					buffer,
					0, VK_WHOLE_SIZE);
				src_pipeline_stage_flags|= src_sync_info->pipeline_stage_flags;
				dst_pipeline_stage_flags|= dst_sync_info->pipeline_stage_flags;
			}
		}
	}

	for(const ImageInfo image_info : params.input_images)
	{
		if(GetLastImageUsage(image_info.image) != ImageUsage::GraphicsSrc)
		{
			const auto sync_info= GetSyncInfoForLastImageUsage(image_info.image);
			image_barriers.emplace_back(
				sync_info.access_flags, vk::AccessFlagBits::eShaderRead,
				sync_info.layout, vk::ImageLayout::eShaderReadOnlyOptimal,
				queue_family_index_, queue_family_index_,
				image_info.image,
				vk::ImageSubresourceRange(image_info.asppect_flags, 0u, image_info.num_mips, 0u, image_info.num_layers));

			src_pipeline_stage_flags|= sync_info.pipeline_stage_flags;
			// Do not allow to execute vertex shader before image isn't ready, since image reads are possible already in vertex shader.
			dst_pipeline_stage_flags|= vk::PipelineStageFlagBits::eVertexShader;
		}
	}

	if(!buffer_barriers.empty() || !image_barriers.empty())
		command_buffer_.pipelineBarrier(
			src_pipeline_stage_flags,
			dst_pipeline_stage_flags,
			vk::DependencyFlags(),
			{},
			buffer_barriers,
			image_barriers);

	// TODO - add synchronization for output images to ensure write after read.

	command_buffer_.beginRenderPass(
		vk::RenderPassBeginInfo(
			params.render_pass,
			params.framebuffer,
			vk::Rect2D(vk::Offset2D(0, 0), params.viewport_size),
			uint32_t(params.clear_values.size()), params.clear_values.data()),
		vk::SubpassContents::eInline);

	func(command_buffer_);

	command_buffer_.endRenderPass();

	UpdateLastBuffersUsage(params.indirect_draw_buffers, BufferUsage::IndirectDrawSrc);
	UpdateLastBuffersUsage(params.index_buffers, BufferUsage::IndexSrc);
	UpdateLastBuffersUsage(params.vertex_buffers, BufferUsage::VertexSrc);
	UpdateLastBuffersUsage(params.uniform_buffers, BufferUsage::UniformSrc);

	for(const ImageInfo& image_info : params.input_images)
		UpdateLastImageUsage(image_info.image, ImageUsage::GraphicsSrc);
}

void TaskOrganizer::ExecuteTask(const TransferTaskParams& params, const TaskFunc& func)
{
	std::vector<vk::BufferMemoryBarrier> buffer_barriers;
	std::vector<vk::ImageMemoryBarrier> image_barriers;
	vk::PipelineStageFlags src_pipeline_stage_flags;
	vk::PipelineStageFlags dst_pipeline_stage_flags;
	bool require_barrier_for_write_after_read_sync= false;

	if(const auto dst_sync_info= GetBufferDstSyncInfo(BufferUsage::TransferSrc))
	{
		for(const vk::Buffer buffer : params.input_buffers)
		{
			if(const auto src_sync_info= GetBufferSrcSyncInfoForLastUsage(buffer))
			{
				buffer_barriers.emplace_back(
					src_sync_info->access_flags, dst_sync_info->access_flags,
					queue_family_index_, queue_family_index_,
					buffer,
					0, VK_WHOLE_SIZE);
				src_pipeline_stage_flags|= src_sync_info->pipeline_stage_flags;
				dst_pipeline_stage_flags|= dst_sync_info->pipeline_stage_flags;
			}
		}
	}

	for(const vk::Buffer buffer : params.output_buffers)
	{
		if(const auto last_usage= GetLastBufferUsage(buffer))
		{
			if(IsReadBufferUsage(*last_usage))
			{
				require_barrier_for_write_after_read_sync= true;
				src_pipeline_stage_flags|= GetPipelineStageForBufferUsage(*last_usage);
				dst_pipeline_stage_flags|= vk::PipelineStageFlagBits::eTransfer;
			}
		}
	}

	for(const ImageInfo image_info : params.input_images)
	{
		if(GetLastImageUsage(image_info.image) != ImageUsage::TransferSrc)
		{
			const auto sync_info= GetSyncInfoForLastImageUsage(image_info.image);
			image_barriers.emplace_back(
				sync_info.access_flags, vk::AccessFlagBits::eTransferRead,
				sync_info.layout, vk::ImageLayout::eTransferSrcOptimal,
				queue_family_index_, queue_family_index_,
				image_info.image,
				vk::ImageSubresourceRange(image_info.asppect_flags, 0u, image_info.num_mips, 0u, image_info.num_layers));

			src_pipeline_stage_flags|= sync_info.pipeline_stage_flags;
			dst_pipeline_stage_flags|= vk::PipelineStageFlagBits::eTransfer;
		}
	}

	for(const ImageInfo& image_info : params.output_images)
	{
		if(GetLastImageUsage(image_info.image) != ImageUsage::TransferDst)
		{
			const auto sync_info= GetSyncInfoForLastImageUsage(image_info.image);
			image_barriers.emplace_back(
				sync_info.access_flags, vk::AccessFlagBits::eTransferWrite,
				sync_info.layout, vk::ImageLayout::eTransferDstOptimal,
				queue_family_index_, queue_family_index_,
				image_info.image,
				vk::ImageSubresourceRange(image_info.asppect_flags, 0u, image_info.num_mips, 0u, image_info.num_layers));

			src_pipeline_stage_flags|= sync_info.pipeline_stage_flags;
			dst_pipeline_stage_flags|= vk::PipelineStageFlagBits::eTransfer;
		}
	}

	if(!buffer_barriers.empty() || !image_barriers.empty() || require_barrier_for_write_after_read_sync)
		command_buffer_.pipelineBarrier(
			src_pipeline_stage_flags,
			dst_pipeline_stage_flags,
			vk::DependencyFlags(),
			{},
			buffer_barriers,
			image_barriers);

	func(command_buffer_);

	UpdateLastBuffersUsage(params.input_buffers, BufferUsage::TransferSrc);
	UpdateLastBuffersUsage(params.output_buffers, BufferUsage::TransferDst);

	for(const ImageInfo& image_info : params.input_images)
		UpdateLastImageUsage(image_info.image, ImageUsage::TransferSrc);
	for(const ImageInfo& image_info : params.output_images)
		UpdateLastImageUsage(image_info.image, ImageUsage::TransferDst);
}

void TaskOrganizer::GenerateImageMips(const ImageInfo& image_info, const vk::Extent2D image_size)
{
	// First transfer the whole image to TransferSrcOptimal layout.
	if(GetLastImageUsage(image_info.image) != ImageUsage::TransferSrc)
	{
		const auto sync_info= GetSyncInfoForLastImageUsage(image_info.image);
		const vk::ImageMemoryBarrier image_barrier(
			sync_info.access_flags, vk::AccessFlagBits::eTransferRead,
			sync_info.layout, vk::ImageLayout::eTransferSrcOptimal,
			queue_family_index_, queue_family_index_,
			image_info.image,
			vk::ImageSubresourceRange(image_info.asppect_flags, 0u, image_info.num_mips, 0u, image_info.num_layers));

		command_buffer_.pipelineBarrier(
			sync_info.pipeline_stage_flags,
			 vk::PipelineStageFlagBits::eTransfer,
			vk::DependencyFlags(),
			0u, nullptr,
			0u, nullptr,
			1u, &image_barrier);
	}

	// Pefrom blitting for mip chain.
	for(uint32_t i= 1; i < image_info.num_mips; ++i)
	{
		// Transfer destination mip to TransferDstOptimal layout.
		{
			const vk::ImageMemoryBarrier image_barrier(
				vk::AccessFlagBits(), vk::AccessFlagBits::eTransferWrite,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
				queue_family_index_, queue_family_index_,
				image_info.image,
				vk::ImageSubresourceRange(image_info.asppect_flags, i, 1, 0u, image_info.num_layers));

			command_buffer_.pipelineBarrier(
				vk::PipelineStageFlagBits(),
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				0u, nullptr,
				0u, nullptr,
				1u, &image_barrier);
		}

		// Perform blitting with linear interpolation.
		// This effectively avegages pixels in 2x2 block.

		const vk::ImageBlit image_blit(
			vk::ImageSubresourceLayers(image_info.asppect_flags, i - 1, 0u, image_info.num_layers),
			{
				vk::Offset3D(0, 0, 0),
				vk::Offset3D(image_size.width >> (i - 1), image_size.height >> (i - 1), 1),
			},
			vk::ImageSubresourceLayers(image_info.asppect_flags, i, 0u, image_info.num_layers),
			{
				vk::Offset3D(0, 0, 0),
				vk::Offset3D(image_size.width >> i, image_size.height >> i, 1),
			});

		command_buffer_.blitImage(
			image_info.image,
			vk::ImageLayout::eTransferSrcOptimal,
			image_info.image,
			vk::ImageLayout::eTransferDstOptimal,
			1u, &image_blit,
			vk::Filter::eLinear);

		// Transfer this mip to TransferSrcOptimal layout.
		{
			const vk::ImageMemoryBarrier image_barrier(
				vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
				queue_family_index_, queue_family_index_,
				image_info.image,
				vk::ImageSubresourceRange(image_info.asppect_flags, i, 1, 0u, image_info.num_layers));

			command_buffer_.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				0u, nullptr,
				0u, nullptr,
				1u, &image_barrier);
		}
	}

	// At the end of the mip chain generation all image mips should be in TransferSrc layout.
	// Mark this as image TransferSrc usage.
	UpdateLastImageUsage(image_info.image, ImageUsage::TransferSrc);
}

void TaskOrganizer::UpdateLastBuffersUsage(const std::vector<vk::Buffer> buffers, const BufferUsage usage)
{
	for(const vk::Buffer buffer : buffers)
		UpdateLastBufferUsage(buffer, usage);
}

void TaskOrganizer::UpdateLastBufferUsage(const vk::Buffer buffer, const BufferUsage usage)
{
	last_buffer_usage_[buffer]= usage;
}

std::optional<TaskOrganizer::BufferUsage> TaskOrganizer::GetLastBufferUsage(const vk::Buffer buffer) const
{
	const auto it= last_buffer_usage_.find(buffer);
	if(it == last_buffer_usage_.end())
		return std::nullopt;
	return it->second;
}

void TaskOrganizer::UpdateLastImageUsage(const vk::Image image, const ImageUsage usage)
{
	last_image_usage_[image]= usage;
}

std::optional<TaskOrganizer::ImageUsage> TaskOrganizer::GetLastImageUsage(const vk::Image image) const
{
	const auto it= last_image_usage_.find(image);
	if(it == last_image_usage_.end())
		return std::nullopt;
	return it->second;
}

std::optional<TaskOrganizer::BufferSyncInfo> TaskOrganizer::GetBufferSrcSyncInfoForLastUsage(const vk::Buffer buffer) const
{
	if(const auto last_usage= GetLastBufferUsage(buffer))
		return GetBufferSrcSyncInfo(*last_usage);

	return std::nullopt;
}

std::optional<TaskOrganizer::BufferSyncInfo> TaskOrganizer::GetBufferSrcSyncInfo(const BufferUsage usage)
{
	switch(usage)
	{
	// Require no synchronization if previous usage is constant.
	case BufferUsage::IndirectDrawSrc:
	case BufferUsage::IndexSrc:
	case BufferUsage::VertexSrc:
	case BufferUsage::UniformSrc:
	case BufferUsage::ComputeShaderSrc:
	case BufferUsage::TransferSrc:
		return std::nullopt;

	case BufferUsage::ComputeShaderDst:
		return BufferSyncInfo{vk::AccessFlagBits::eShaderWrite, vk::PipelineStageFlagBits::eComputeShader};

	case BufferUsage::TransferDst:
		return BufferSyncInfo{vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer};
	};
	HEX_ASSERT(false);
	return std::nullopt;
}

std::optional<TaskOrganizer::BufferSyncInfo> TaskOrganizer::GetBufferDstSyncInfo(const BufferUsage usage)
{
	// TODO - check if this is correct.
	switch(usage)
	{
	case BufferUsage::IndirectDrawSrc:
		return BufferSyncInfo{vk::AccessFlagBits::eIndirectCommandRead, vk::PipelineStageFlagBits::eDrawIndirect};

	case BufferUsage::IndexSrc:
		return BufferSyncInfo{vk::AccessFlagBits::eIndexRead, vk::PipelineStageFlagBits::eVertexInput};

	case BufferUsage::VertexSrc:
		return BufferSyncInfo{vk::AccessFlagBits::eVertexAttributeRead, vk::PipelineStageFlagBits::eVertexInput};

	case BufferUsage::UniformSrc:
		// Uniforms may be accessed in vertex shader and later.
		return BufferSyncInfo{vk::AccessFlagBits::eUniformRead, vk::PipelineStageFlagBits::eVertexShader};

	case BufferUsage::ComputeShaderSrc:
		return BufferSyncInfo{vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eComputeShader};

	case BufferUsage::TransferSrc:
		return BufferSyncInfo{vk::AccessFlagBits::eTransferRead, vk::PipelineStageFlagBits::eTransfer};

	// Require no synchronization if this is a destination buffer.
	case BufferUsage::ComputeShaderDst:
	case BufferUsage::TransferDst:
		return std::nullopt;
	};
	HEX_ASSERT(false);
	return std::nullopt;
}

bool TaskOrganizer::IsReadBufferUsage(const BufferUsage usage)
{
	switch(usage)
	{
	case BufferUsage::IndirectDrawSrc:
	case BufferUsage::IndexSrc:
	case BufferUsage::VertexSrc:
	case BufferUsage::UniformSrc:
	case BufferUsage::ComputeShaderSrc:
	case BufferUsage::TransferSrc:
		return true;

	case BufferUsage::ComputeShaderDst:
	case BufferUsage::TransferDst:
		return false;
	};

	HEX_ASSERT(false);
	return false;
}

vk::PipelineStageFlags TaskOrganizer::GetPipelineStageForBufferUsage(const BufferUsage usage)
{
	// TODO - check if this is correct.
	switch(usage)
	{
	case BufferUsage::IndirectDrawSrc:
		return vk::PipelineStageFlagBits::eDrawIndirect;
	case BufferUsage::IndexSrc:
		return vk::PipelineStageFlagBits::eVertexInput;
	case BufferUsage::VertexSrc:
		return vk::PipelineStageFlagBits::eVertexInput;
	case BufferUsage::UniformSrc:
		// TODO - list other kinds of shaders?
		return vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eGeometryShader | vk::PipelineStageFlagBits::eFragmentShader;
	case BufferUsage::ComputeShaderSrc:
	case BufferUsage::ComputeShaderDst:
		return vk::PipelineStageFlagBits::eComputeShader;
	case BufferUsage::TransferSrc:
	case BufferUsage::TransferDst:
		return vk::PipelineStageFlagBits::eTransfer;
	};

	HEX_ASSERT(false);
	return vk::PipelineStageFlags();
}

TaskOrganizer::ImageSyncInfo TaskOrganizer::GetSyncInfoForLastImageUsage(const vk::Image image)
{
	if(const auto usage= GetLastImageUsage(image))
		return GetSyncInfoForImageUsage(*usage);
	return {vk::AccessFlags(), vk::PipelineStageFlagBits::eBottomOfPipe, vk::ImageLayout::eUndefined};
}

TaskOrganizer::ImageSyncInfo TaskOrganizer::GetSyncInfoForImageUsage(const ImageUsage usage)
{
	switch(usage)
	{
	case ImageUsage::GraphicsSrc:
		// TODO - list other kinds of shaders?
		return {vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eGeometryShader | vk::PipelineStageFlagBits::eFragmentShader, vk::ImageLayout::eShaderReadOnlyOptimal};
	case ImageUsage::TransferDst:
		return {vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer, vk::ImageLayout::eTransferDstOptimal};
	case ImageUsage::TransferSrc:
		return {vk::AccessFlagBits::eTransferRead, vk::PipelineStageFlagBits::eTransfer, vk::ImageLayout::eTransferSrcOptimal};
	case ImageUsage::ComputeDst:
		return {vk::AccessFlagBits::eShaderWrite, vk::PipelineStageFlagBits::eComputeShader, vk::ImageLayout::eGeneral};
	};

	HEX_ASSERT(false);
	return {};
}

} // namespace HexGPU
