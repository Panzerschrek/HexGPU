#include "TaskOrganiser.hpp"
#include "Assert.hpp"

namespace HexGPU
{

TaskOrganiser::TaskOrganiser(WindowVulkan& window_vulkan)
	: queue_family_index_(window_vulkan.GetQueueFamilyIndex())
{
}

void TaskOrganiser::AddTask(Task task)
{
	tasks_.push_back(std::move(task));
}

void TaskOrganiser::ExecuteTasks(const vk::CommandBuffer command_buffer)
{
	for(const Task& task : tasks_)
		std::visit([&](const auto& t){ ExecuteTaskImpl(command_buffer, t); }, task);

	tasks_.clear();

	// Preserve last buffer usages in order to insert proper barriers,
	// if these buffers are used in next frame when this frame is in-fly.
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const ComputeTask& task)
{
	std::vector<vk::BufferMemoryBarrier> buffer_barriers;
	vk::PipelineStageFlags src_pipeline_stage_flags;
	vk::PipelineStageFlags dst_pipeline_stage_flags;

	if(const auto dst_sync_info= GetBufferDstSyncInfo(BufferUsage::ComputeShaderSrc))
	{
		for(const vk::Buffer buffer : task.input_storage_buffers)
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

		for(const vk::Buffer buffer : task.input_output_storage_buffers)
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

	// TODO - add synchronization of destination buffers to ensure write only after read.

	if(!buffer_barriers.empty())
		command_buffer.pipelineBarrier(
			src_pipeline_stage_flags,
			dst_pipeline_stage_flags,
			vk::DependencyFlags(),
			0, nullptr,
			uint32_t(buffer_barriers.size()), buffer_barriers.data(),
			0, nullptr);

	task.func(command_buffer);

	UpdateLastBuffersUsage(task.input_storage_buffers, BufferUsage::ComputeShaderSrc);
	UpdateLastBuffersUsage(task.output_storage_buffers, BufferUsage::ComputeShaderDst);
	UpdateLastBuffersUsage(task.input_output_storage_buffers, BufferUsage::ComputeShaderDst);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const GraphicsTask& task)
{
	std::vector<vk::BufferMemoryBarrier> buffer_barriers;
	vk::PipelineStageFlags src_pipeline_stage_flags;
	vk::PipelineStageFlags dst_pipeline_stage_flags;

	if(const auto dst_sync_info= GetBufferDstSyncInfo(BufferUsage::IndirectDrawSrc))
	{
		for(const vk::Buffer buffer : task.indirect_draw_buffers)
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
		for(const vk::Buffer buffer : task.index_buffers)
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
		for(const vk::Buffer buffer : task.vertex_buffers)
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
		for(const vk::Buffer buffer : task.uniform_buffers)
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

	if(!buffer_barriers.empty())
		command_buffer.pipelineBarrier(
			src_pipeline_stage_flags,
			dst_pipeline_stage_flags,
			vk::DependencyFlags(),
			0, nullptr,
			uint32_t(buffer_barriers.size()), buffer_barriers.data(),
			0, nullptr);

	// TODO - add synchronization for output images to ensure write after read.

	task.func(command_buffer);

	UpdateLastBuffersUsage(task.indirect_draw_buffers, BufferUsage::IndirectDrawSrc);
	UpdateLastBuffersUsage(task.index_buffers, BufferUsage::IndexSrc);
	UpdateLastBuffersUsage(task.vertex_buffers, BufferUsage::VertexSrc);
	UpdateLastBuffersUsage(task.uniform_buffers, BufferUsage::UniformSrc);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const TransferTask& task)
{
	std::vector<vk::BufferMemoryBarrier> buffer_barriers;
	vk::PipelineStageFlags src_pipeline_stage_flags;
	vk::PipelineStageFlags dst_pipeline_stage_flags;

	if(const auto dst_sync_info= GetBufferDstSyncInfo(BufferUsage::TransferSrc))
	{
		for(const vk::Buffer buffer : task.input_buffers)
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

	// TODO - add synchronization of destination buffers to ensure write only after read.

	if(!buffer_barriers.empty())
		command_buffer.pipelineBarrier(
			src_pipeline_stage_flags,
			dst_pipeline_stage_flags,
			vk::DependencyFlags(),
			0, nullptr,
			uint32_t(buffer_barriers.size()), buffer_barriers.data(),
			0, nullptr);

	task.func(command_buffer);

	UpdateLastBuffersUsage(task.input_buffers, BufferUsage::TransferSrc);
	UpdateLastBuffersUsage(task.output_buffers, BufferUsage::TransferDst);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const PresentTask& task)
{
	// TODO - add barriers.

	task.func(command_buffer);
}

void TaskOrganiser::UpdateLastBuffersUsage(const std::vector<vk::Buffer> buffers, const BufferUsage usage)
{
	for(const vk::Buffer buffer : buffers)
		UpdateLastBufferUsage(buffer, usage);
}

void TaskOrganiser::UpdateLastBufferUsage(const vk::Buffer buffer, const BufferUsage usage)
{
	last_buffer_usage_[buffer]= usage;
}

std::optional<TaskOrganiser::BufferUsage> TaskOrganiser::GetLastBufferUsage(const vk::Buffer buffer) const
{
	const auto it= last_buffer_usage_.find(buffer);
	if(it == last_buffer_usage_.end())
		return std::nullopt;
	return it->second;
}

std::optional<TaskOrganiser::BufferSyncInfo> TaskOrganiser::GetBufferSrcSyncInfoForLastUsage(const vk::Buffer buffer) const
{
	if(const auto last_usage= GetLastBufferUsage(buffer))
		return GetBufferSrcSyncInfo(*last_usage);

	return std::nullopt;
}

std::optional<TaskOrganiser::BufferSyncInfo> TaskOrganiser::GetBufferSrcSyncInfo(const BufferUsage usage)
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

std::optional<TaskOrganiser::BufferSyncInfo> TaskOrganiser::GetBufferDstSyncInfo(const BufferUsage usage)
{
	// TODO - check if this is correct.
	switch(usage)
	{
	case BufferUsage::IndirectDrawSrc:
		return BufferSyncInfo{vk::AccessFlagBits::eIndirectCommandRead, vk::PipelineStageFlagBits::eDrawIndirect};

	case BufferUsage::IndexSrc:
		return BufferSyncInfo{vk::AccessFlagBits::eIndexRead, vk::PipelineStageFlagBits::eVertexInput};

	case BufferUsage::VertexSrc:
		return BufferSyncInfo{vk::AccessFlagBits::eVertexAttributeRead, vk::PipelineStageFlagBits::eVertexShader};

	case BufferUsage::UniformSrc:
		return BufferSyncInfo{vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eVertexShader};

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

} // namespace HexGPU
