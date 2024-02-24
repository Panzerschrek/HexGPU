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
	, allocator_data_buffer_size_(CalculateAllocatorDataSize(total_memory_units))
{
	allocator_data_buffer_=
		vk_device_.createBufferUnique(
			vk::BufferCreateInfo(
				vk::BufferCreateFlags(),
				allocator_data_buffer_size_,
				vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));

	const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*allocator_data_buffer_);

	const auto memory_properties= window_vulkan.GetMemoryProperties();

	vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
	for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
	{
		if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
		{
			memory_allocate_info.memoryTypeIndex= i;
			break;
		}
	}

	allocator_data_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
	vk_device_.bindBufferMemory(*allocator_data_buffer_, *allocator_data_buffer_memory_, 0u);
}

GPUAllocator::~GPUAllocator()
{
	vk_device_.waitIdle();
}

vk::Buffer GPUAllocator::GetAllocatorDataBuffer() const
{
	return allocator_data_buffer_.get();
}

uint32_t GPUAllocator::GetAllocatorDataBufferSize() const
{
	return allocator_data_buffer_size_;
}

void GPUAllocator::EnsureInitialized(const vk::CommandBuffer command_buffer)
{
	if(initialized_)
		return;
	initialized_= true;

	std::vector<uint32_t> data;
	data.resize(allocator_data_buffer_size_ / sizeof(uint32_t), uint32_t(0)); // Fill with zeros - indicating free memory.
	data[0]= total_memory_units_; // Set size.

	command_buffer.updateBuffer(*allocator_data_buffer_, 0u, allocator_data_buffer_size_, data.data());

	// Create barrier between update allocator data buffer and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			*allocator_data_buffer_,
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

} // namespace HexGPU
