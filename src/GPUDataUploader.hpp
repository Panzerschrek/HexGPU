#pragma once
#include "Buffer.hpp"

namespace HexGPU
{

// A helper class for data uploading into GPU buffers, which are not host-visible.
class GPUDataUploader
{
public:
	explicit GPUDataUploader(WindowVulkan& window_vulkan);
	~GPUDataUploader();

	// Copy data from host to destination buffer.
	// Destination buffer should have TransferDst usage bit set.
	// This requires CPU<->GPU synchronization and generally should be used to upload data only on startup.
	void UploadData(const void* data, vk::DeviceSize size, vk::Buffer dst_buffer, uint32_t dst_offset);

private:
	const vk::Device vk_device_;

	const vk::Queue queue_;
	const vk::UniqueCommandBuffer command_buffer_;

	const Buffer buffer_;
	void* const buffer_mapped_;
};

} // namespace HexGPU
