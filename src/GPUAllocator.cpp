#include "GPUAllocator.hpp"

namespace HexGPU
{

namespace
{

// Structs same as in GLSL code. If this changed, GLSL code must be changed too!

struct Allocator
{
	uint32_t total_memory_size_units;
	uint32_t max_regions;
	uint32_t first_region_index;
	uint32_t first_free_region_index;
	uint32_t first_available_region_index;
};

struct AllocatorLinkedListNode
{
	uint32_t prev_index;
	uint32_t next_index;
};

struct AllocatorMemoryRegion
{
	AllocatorLinkedListNode regions_sequence_list_node;
	AllocatorLinkedListNode free_regions_list_node;
	AllocatorLinkedListNode available_regions_list_node;
	uint offset_units;
	uint size_units;
	bool is_used;
};

} // namespace

GPUAllocator::GPUAllocator(
	WindowVulkan& window_vulkan,
	const uint32_t total_memory_units,
	const uint32_t max_regions)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, total_memory_units_(total_memory_units)
	, max_regions_(max_regions)
	// TODO - check if we can place AllocatorMemoryRegion array after Allocator struct without any padding.
	, allocator_data_buffer_size_(uint32_t(sizeof(Allocator) + sizeof(AllocatorMemoryRegion) * max_regions_))
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

void GPUAllocator::EnsureInitialized(const vk::CommandBuffer command_buffer)
{
	if(initialized_)
		return;
	initialized_= true;

	std::vector<uint8_t> data;
	data.resize(allocator_data_buffer_size_, uint8_t(0));

	auto& allocator_header= *reinterpret_cast<Allocator*>(data.data());
	allocator_header.total_memory_size_units= total_memory_units_;
	allocator_header.max_regions= max_regions_;

	// TODO - setup linked list heads properly.
	allocator_header.first_region_index= 0;
	allocator_header.first_free_region_index= 0;
	allocator_header.first_available_region_index= 0;

	// TODO - fill regions.

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
