#pragma once
#include "WindowVulkan.hpp"
#include <functional>
#include <unordered_map>
#include <variant>

namespace HexGPU
{

class TaskOrganiser
{
public:
	struct ImageInfo
	{
		vk::Image image;
		vk::ImageAspectFlags asppect_flags;
		uint32_t num_mips= 0;
		uint32_t num_layers= 0;
	};

	using TaksFunc= std::function<void(vk::CommandBuffer command_buffer)>;

	struct TaskBase
	{
		TaksFunc func;
	};

	struct ComputeTask : public TaskBase
	{
		std::vector<vk::Buffer> input_storage_buffers;
		std::vector<vk::Buffer> output_storage_buffers;
		// Buffers which are both input and output. Do not list them in input and/or output lists.
		std::vector<vk::Buffer> input_output_storage_buffers;
	};

	struct GraphicsTask : public TaskBase
	{
		// Input graphics buffers.
		std::vector<vk::Buffer> indirect_draw_buffers;
		std::vector<vk::Buffer> index_buffers;
		std::vector<vk::Buffer> vertex_buffers;
		std::vector<vk::Buffer> uniform_buffers;

		std::vector<ImageInfo> input_images;

		vk::Framebuffer framebuffer;
		vk::Extent2D viewport_size;
		vk::RenderPass render_pass;
		std::vector<vk::ClearValue> clear_values;
	};

	struct TransferTask : public TaskBase
	{
		std::vector<vk::Buffer> input_buffers;
		std::vector<vk::Buffer> output_buffers;
		std::vector<ImageInfo> input_images;
		std::vector<ImageInfo> output_images;
	};

	struct PresentTask : public TaskBase
	{
		// TODO
	};

	using Task= std::variant<ComputeTask, GraphicsTask, TransferTask, PresentTask>;

public:
	explicit TaskOrganiser(WindowVulkan& window_vulkan);

	void SetCommandBuffer(vk::CommandBuffer command_buffer);

	// Push commands of the task into the command buffer.
	// Passed function is executed immideately.
	void ExecuteTask(const Task& task);

private:
	enum struct BufferUsage : uint8_t
	{
		IndirectDrawSrc,
		IndexSrc,
		VertexSrc,
		UniformSrc,
		ComputeShaderSrc,
		ComputeShaderDst,
		TransferDst,
		TransferSrc,
	};

	struct BufferSyncInfo
	{
		vk::AccessFlags access_flags;
		vk::PipelineStageFlags pipeline_stage_flags;
	};

	enum class ImageUsage : uint8_t
	{
		GraphicsSrc,
		TransferDst,
		TransferSrc,
	};

	struct ImageSyncInfo
	{
		vk::AccessFlags access_flags;
		vk::PipelineStageFlags pipeline_stage_flags;
		vk::ImageLayout layout= vk::ImageLayout::eUndefined;
	};

private:
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const ComputeTask& task);
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const GraphicsTask& task);
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const TransferTask& task);
	void ExecuteTaskImpl(vk::CommandBuffer command_buffer, const PresentTask& task);

	void UpdateLastBuffersUsage(const std::vector<vk::Buffer> buffers, BufferUsage usage);
	void UpdateLastBufferUsage(vk::Buffer buffer, BufferUsage usage);
	std::optional<BufferUsage> GetLastBufferUsage(vk::Buffer buffer) const;

	void UpdateLastImageUsage(vk::Image image, ImageUsage usage);
	std::optional<ImageUsage> GetLastImageUsage(vk::Image image) const;

	std::optional<BufferSyncInfo> GetBufferSrcSyncInfoForLastUsage(vk::Buffer buffer) const;

	static std::optional<BufferSyncInfo> GetBufferSrcSyncInfo(BufferUsage usage);
	static std::optional<BufferSyncInfo> GetBufferDstSyncInfo(BufferUsage usage);
	static bool IsReadBufferUsage(BufferUsage usage);
	static vk::PipelineStageFlags GetPipelineStageForBufferUsage(BufferUsage usage);

	ImageSyncInfo GetSyncInfoForLastImageUsage(vk::Image image);

	static ImageSyncInfo GetSyncInfoForImageUsage(ImageUsage usage);

private:
	const uint32_t queue_family_index_;

	vk::CommandBuffer command_buffer_;

	// Remember buffer usages in order to setup barriers properly.
	std::unordered_map<VkBuffer, BufferUsage> last_buffer_usage_;
	std::unordered_map<VkImage, ImageUsage> last_image_usage_;
};

} // namespace HexGPU
