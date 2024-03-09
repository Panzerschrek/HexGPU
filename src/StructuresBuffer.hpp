#pragma once
#include "Buffer.hpp"
#include "BlockType.hpp"

namespace HexGPU
{

class StructuresBuffer
{
public:
	// This struct must match the same struct in GLSL code!
	struct StructureDescription
	{
		uint8_t size[4]{};
		uint32_t data_offset= 0;
	};

	struct Structures
	{
		std::vector<StructuresBuffer::StructureDescription> descriptions;
		std::vector<BlockType> data;
	};

public:
	StructuresBuffer(WindowVulkan& window_vulkan, const Structures& structures);
	~StructuresBuffer();

	vk::Buffer GetDescriptionsBuffer() const;
	vk::DeviceSize GetDescriptionsBufferSize() const;

	vk::Buffer GetDataBuffer() const;
	vk::DeviceSize GetDataBufferSize() const;

private:
	const vk::Device vk_device_;

	const Buffer description_buffer_;
	const Buffer data_buffer_;
};

} // namespace HexGPU
