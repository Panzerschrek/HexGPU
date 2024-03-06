#include "GPUAllocator.hpp"

namespace HexGPU
{

namespace
{

uint32_t CalculateAllocatorDataSize(const uint32_t total_memory_units)
{
	const uint32_t num_blocks= (total_memory_units + 31) / 32; // Round-up.
	// 1 uint for size and uints for blocks.
	return uint32_t(sizeof(uint32_t)) + num_blocks * uint32_t(sizeof(uint32_t));
}

} // namespace

GPUAllocator::GPUAllocator(WindowVulkan& window_vulkan, const uint32_t total_memory_units)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, total_memory_units_(total_memory_units)
	, allocator_data_buffer_(
		window_vulkan,
		CalculateAllocatorDataSize(total_memory_units),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
{
}

GPUAllocator::~GPUAllocator()
{
	vk_device_.waitIdle();
}

vk::Buffer GPUAllocator::GetAllocatorDataBuffer() const
{
	return allocator_data_buffer_.GetBuffer();
}

vk::DeviceSize GPUAllocator::GetAllocatorDataBufferSize() const
{
	return allocator_data_buffer_.GetSize();
}

void GPUAllocator::EnsureInitialized(TaskOrganiser& task_organiser)
{
	if(initialized_)
		return;
	initialized_= true;

	TaskOrganiser::TransferTask task;
	task.output_buffers.push_back(allocator_data_buffer_.GetBuffer());

	task.func=
		[this](const vk::CommandBuffer command_buffer)
		{
			std::vector<uint32_t> data;
			data.resize(allocator_data_buffer_.GetSize() / sizeof(uint32_t), uint32_t(0)); // Fill with zeros - indicating free memory.
			data[0]= total_memory_units_; // Set size.

			command_buffer.updateBuffer(allocator_data_buffer_.GetBuffer(), 0u, allocator_data_buffer_.GetSize(), data.data());
		};

	task_organiser.AddTask(std::move(task));
}

} // namespace HexGPU
