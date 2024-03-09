#pragma once
#include "WindowVulkan.hpp"
#include <functional>
#include <optional>
#include <unordered_map>

namespace HexGPU
{

/*
	A class wich simplifies synchronization of Vulakn tasks between each other.
	All task are started in submission order.
	Each task has inputs and outputs - buffers and images.
	If a buffer is used as input in one task, a barrier may be added before it in order to ensure,
	that previous tasks which use this buffer are finished (if necessary).
	The same is performed for images too.
	Also this class manages image layout transitions.
	So, it's almost not needed to use manual pipeline barriers.

	Graphics tasks also starn/end render passes.

	It's imprortant not to forget to specify all inputs/outputs in order to ensure proper synchronization.
	So, when modifying tasks code make sure new inputs/outputs are properly added.

	TODO - improve this class:
	* Add output images in render passes
	* Add output images in compute tasks
	* Allow render passes to perform image layout transition themselves and track images layout separately
*/
class TaskOrganizer
{
public:
	struct ImageInfo
	{
		vk::Image image;
		vk::ImageAspectFlags asppect_flags;
		uint32_t num_mips= 0;
		uint32_t num_layers= 0;
	};

	using TaskFunc= std::function<void(vk::CommandBuffer command_buffer)>;

	// Task type for a batch of compute shader dispatches.
	struct ComputeTaskParams
	{
		std::vector<vk::Buffer> input_storage_buffers;
		std::vector<vk::Buffer> output_storage_buffers;
		// Buffers which are both input and output. Do not list them in input and/or output lists!
		std::vector<vk::Buffer> input_output_storage_buffers;
	};

	// Task type for a batch of draw commands within single render pass.
	struct GraphicsTaskParams
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

	// Task type for transfer commands - buffer to buffer, buffer to image, image to buffer, image to image copies, buffer updates.
	struct TransferTaskParams
	{
		std::vector<vk::Buffer> input_buffers;
		std::vector<vk::Buffer> output_buffers;
		std::vector<ImageInfo> input_images;
		std::vector<ImageInfo> output_images;
	};

public:
	explicit TaskOrganizer(WindowVulkan& window_vulkan);

	// Set current command buffer. Initially there is no buffer.
	void SetCommandBuffer(vk::CommandBuffer command_buffer);

	// Execute tasks of different kind.
	void ExecuteTask(const ComputeTaskParams& params, const TaskFunc& func);
	void ExecuteTask(const GraphicsTaskParams& params, const TaskFunc& func);
	void ExecuteTask(const TransferTaskParams& params, const TaskFunc& func);

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
		// TODO - add GraphicsDst for render pass output images.
		// TODO - add usages for compute shaders.
	};

	struct ImageSyncInfo
	{
		vk::AccessFlags access_flags;
		vk::PipelineStageFlags pipeline_stage_flags;
		vk::ImageLayout layout= vk::ImageLayout::eUndefined;
	};

private:
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
