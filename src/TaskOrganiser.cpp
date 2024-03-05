#include "TaskOrganiser.hpp"

namespace HexGPU
{

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
	// TODO - add barriers.

	task.func(command_buffer);

	// Process input buffers, than output buffers to handle in-out buffers properly. Last usage in such case should be write.
	UpdateLastBuffersUsage(task.input_storage_buffers, BufferUsage::ComputeShaderSrc);
	UpdateLastBuffersUsage(task.output_storage_buffers, BufferUsage::ComputeShaderDst);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const GraphicsTask& task)
{
	// TODO - add barriers.

	task.func(command_buffer);

	UpdateLastBuffersUsage(task.indirect_draw_buffers, BufferUsage::IndirectDrawSrc);
	UpdateLastBuffersUsage(task.vertex_buffers, BufferUsage::VertexSrc);
	UpdateLastBuffersUsage(task.index_buffers, BufferUsage::IndexSrc);
	UpdateLastBuffersUsage(task.uniform_buffers, BufferUsage::UniformSrc);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const TransferTask& task)
{
	// TODO - add barriers.

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

} // namespace HexGPU
