#pragma once
#include "BlockType.hpp"
#include <vector>

namespace HexGPU
{

// This struct must match the same struct in GLSL code!
struct StructureDescription
{
	uint8_t size[4]{};
	uint8_t center[4]{}; // Point used for placement.
	uint32_t data_offset= 0;
};

struct Structures
{
	std::vector<StructureDescription> descriptions;
	std::vector<BlockType> data;
};

Structures GenStructures();

} // namespace HexGPU
