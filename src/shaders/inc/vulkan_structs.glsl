// This file contains definitions of some Vulkan structus.
// Definitions should be identical to C++ code.

struct VkDrawIndexedIndirectCommand
{
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int vertexOffset;
	uint firstInstance;
};
