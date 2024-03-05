#pragma once
#include "HexGPUVulkan.hpp"
#include <functional>
#include <variant>

namespace HexGPU
{

class TaskOrganiser
{
public:
	// Avoid capturing non-persistent data into this function (stack variables, args, etc) by reference.
	using TaksFunc= std::function<void(vk::CommandBuffer command_buffer)>;

	struct TaskBase
	{
		TaksFunc func;
	};

	struct ComputeTask : public TaskBase
	{
		std::vector<vk::Buffer> input_storage_buffers;
		std::vector<vk::Buffer> output_storage_buffers;
	};

	struct GraphicsTask : public TaskBase
	{
		// TODO
	};

	struct TransferTask : public TaskBase
	{
		std::vector<vk::Buffer> input_buffers;
		std::vector<vk::Buffer> output_buffers;
	};

	struct PresentTask : public TaskBase
	{
		// TODO
	};

	using Task= std::variant<ComputeTask, GraphicsTask, TransferTask, PresentTask>;

public:
	// Add a task. All tasks are executed in addition order.
	void AddTask(Task task);

	// Execute all tasks.
	void ExecuteTasks(vk::CommandBuffer command_buffer);

private:
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const ComputeTask& task);
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const GraphicsTask& task);
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const TransferTask& task);
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const PresentTask& task);

private:
	std::vector<Task> tasks_;
};

} // namespace HexGPU
