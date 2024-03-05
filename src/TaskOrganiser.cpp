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
	task.func(command_buffer);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const GraphicsTask& task)
{
	task.func(command_buffer);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const TransferTask& task)
{
	task.func(command_buffer);
}

void TaskOrganiser::ExecuteTaskImpl(const vk::CommandBuffer command_buffer, const PresentTask& task)
{
	task.func(command_buffer);
}

} // namespace HexGPU
