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
	{
		std::visit([&](const auto& t){ ExecuteTaskImpl(command_buffer, t); }, task);
	}

	tasks_.clear();
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const ComputeTask& task)
{
	// TODO - add barriers.

	task.func(command_buffer);

	for(const vk::Buffer output_buffer : task.output_storage_buffers)
		UpdateLastBufferUsage(output_buffer, BufferUsage::ComputeShaderReadWrite);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const GraphicsTask& task)
{
	// TODO - add barriers.

	task.func(command_buffer);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const TransferTask& task)
{
	// TODO - add barriers.

	task.func(command_buffer);

	for(const vk::Buffer output_buffer : task.output_buffers)
		UpdateLastBufferUsage(output_buffer, BufferUsage::TransferDst);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const PresentTask& task)
{
	// TODO - add barriers.

	task.func(command_buffer);
}

void TaskOrganiser::UpdateLastBufferUsage(const vk::Buffer buffer, const BufferUsage usage)
{
	last_buffer_usage_[buffer]= usage;
}

} // namespace HexGPU
