#include "GPUDataUploader.hpp"
#include "Assert.hpp"

namespace HexGPU
{

GPUDataUploader::GPUDataUploader(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_(window_vulkan.GetQueue())
	, command_buffer_(std::move(vk_device_.allocateCommandBuffersUnique(
		vk::CommandBufferAllocateInfo(
			window_vulkan.GetCommandPool(),
			vk::CommandBufferLevel::ePrimary,
			1u)).front()))
	, buffer_(
		window_vulkan,
		4 * 1024 * 1024,
		vk::BufferUsageFlagBits::eTransferSrc,
		// This memory visibility is optimal for uploading (shold be pretty fast).
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
	, buffer_mapped_(buffer_.Map(vk_device_))
{
}

GPUDataUploader::~GPUDataUploader()
{
	buffer_.Unmap(vk_device_);
}

void GPUDataUploader::UploadData(
	const void* const data,
	const vk::DeviceSize size,
	const vk::Buffer dst_buffer,
	const uint32_t dst_offset)
{
	HEX_ASSERT(size < buffer_.GetSize()); // TODO - upload larger data blocks in chunks.

	std::memcpy(buffer_mapped_, data, size);

	command_buffer_->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	command_buffer_->copyBuffer(
		buffer_.GetBuffer(),
		dst_buffer,
		{{0, dst_offset, size}});

	command_buffer_->end();

	const vk::PipelineStageFlags wait_dst_stage_mask= vk::PipelineStageFlagBits::eTransfer;
	const vk::SubmitInfo submit_info(
		0u, nullptr,
		&wait_dst_stage_mask,
		1u, &*command_buffer_,
		0u, nullptr);

	queue_.submit(submit_info, vk::Fence());

	// Wait until copying is finished.
	queue_.waitIdle();
}

} // namespace HexGPU
