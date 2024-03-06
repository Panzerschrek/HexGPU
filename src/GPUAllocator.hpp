#include "Buffer.hpp"
#include "TaskOrganiser.hpp"

namespace HexGPU
{

// A class that creates and initializes GPU allocator control structures.
class GPUAllocator
{
public:
	// This should match binding number in shader!
	static constexpr uint32_t c_allocator_buffer_binding= 3333;

public:
	GPUAllocator(WindowVulkan& window_vulkan, uint32_t total_memory_units);
	~GPUAllocator();

	vk::Buffer GetAllocatorDataBuffer() const;
	vk::DeviceSize GetAllocatorDataBufferSize() const;

	void EnsureInitialized(TaskOrganiser& task_organiser);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const uint32_t total_memory_units_;
	const Buffer allocator_data_buffer_;

	bool initialized_= false;
};

} // namespace HexGPU
