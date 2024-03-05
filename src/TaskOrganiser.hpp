#pragma once
#include "HexGPUVulkan.hpp"
#include <functional>
#include <unordered_map>
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
		// If a buffer is both input and output, put it into both containers.
		std::vector<vk::Buffer> input_storage_buffers;
		std::vector<vk::Buffer> output_storage_buffers;
		// TODO - add also images.
	};

	struct GraphicsTask : public TaskBase
	{
		// Input graphics buffers.
		std::vector<vk::Buffer> indirect_draw_buffers;
		std::vector<vk::Buffer> vertex_buffers;
		std::vector<vk::Buffer> index_buffers;
		std::vector<vk::Buffer> uniform_buffers;
		// TODO - add also images.
	};

	struct TransferTask : public TaskBase
	{
		// If a buffer is both input and output, put it into both containers.
		std::vector<vk::Buffer> input_buffers;
		std::vector<vk::Buffer> output_buffers;
		// TODO - add also images.
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
	enum struct BufferUsage : uint8_t
	{
		IndirectDrawSrc,
		VertexSrc,
		IndexSrc,
		UniformSrc,
		ComputeShaderSrc,
		ComputeShaderDst,
		TransferDst,
		TransferSrc,
	};

private:
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const ComputeTask& task);
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const GraphicsTask& task);
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const TransferTask& task);
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const PresentTask& task);

	void UpdateLastBuffersUsage(const std::vector<vk::Buffer> buffers, BufferUsage usage);
	void UpdateLastBufferUsage(vk::Buffer buffer, BufferUsage usage);

private:
	std::vector<Task> tasks_;

	// Remember buffer usages in order to setup barriers properly.
	std::unordered_map<VkBuffer, BufferUsage> last_buffer_usage_;
};

} // namespace HexGPU
