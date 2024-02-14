#include "WorldProcessor.hpp"
#include "Constants.hpp"

namespace HexGPU
{

namespace
{

void FillChunkData(const uint32_t chunk_x, const uint32_t chunk_y, uint8_t* const data)
{
	for(uint32_t x= 0; x < c_chunk_width; ++x)
	for(uint32_t y= 0; y < c_chunk_width; ++y)
	{
		const uint32_t global_x= x + (chunk_x << c_chunk_width_log2);
		const uint32_t global_y= y + (chunk_y << c_chunk_width_log2);
		const uint32_t ground_z= uint32_t(6.0f + 1.5f * std::sin(float(global_x) * 0.5f) + 2.0f * std::sin(float(global_y) * 0.3f));
		for(uint32_t z= 0; z < c_chunk_height; ++z)
		{
			data[ChunkBlockAddress(x, y, z)]= z >= ground_z ? 0 : 1;
		}
	}
}

} // namespace

WorldProcessor::WorldProcessor(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, vk_queue_family_index_(window_vulkan.GetQueueFamilyIndex())
{
	// Create chunk data buffer.
	{
		chunk_data_buffer_size_= c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1];

		vk_chunk_data_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					chunk_data_buffer_size_,
					vk::BufferUsageFlagBits::eStorageBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_chunk_data_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_chunk_data_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_chunk_data_buffer_, *vk_chunk_data_buffer_memory_, 0u);

		// Fil lthe buffer with initial values.
		void* data_gpu_side= nullptr;
		vk_device_.mapMemory(*vk_chunk_data_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &data_gpu_side);

		for(uint32_t x= 0; x < c_chunk_matrix_size[0]; ++x)
		for(uint32_t y= 0; y < c_chunk_matrix_size[1]; ++y)
			FillChunkData(
				x,
				y,
				reinterpret_cast<uint8_t*>(data_gpu_side) + (x + y * c_chunk_matrix_size[0]) * c_chunk_volume);

		vk_device_.unmapMemory(*vk_chunk_data_buffer_memory_);
	}
}

WorldProcessor::~WorldProcessor()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

vk::Buffer WorldProcessor::GetChunkDataBuffer() const
{
	return vk_chunk_data_buffer_.get();
}

uint32_t WorldProcessor::GetChunkDataBufferSize() const
{
	return chunk_data_buffer_size_;
}

} // namespace HexGPU
