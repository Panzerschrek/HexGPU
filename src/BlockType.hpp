#pragma once
#include <cstdint>
#include <string_view>

namespace HexGPU
{

enum class BlockType : uint8_t
{
#define BLOCK_PROCESS_FUNC(x) x,
#include "BlockTypeList.hpp"
#undef BLOCK_PROCESS_FUNC
	NumBlockTypes,
	Unknown,
};

static_assert(uint8_t(BlockType::Air) == 0, "Air must be zero!");

inline BlockType StringToBlockType(const std::string_view s)
{
#define BLOCK_PROCESS_FUNC(x) if(s == #x) return BlockType::x;
#include "BlockTypeList.hpp"
#undef BLOCK_PROCESS_FUNC
	return BlockType::Unknown;
}

inline const char* BlockTypeToString(const BlockType block_type)
{
	switch(block_type)
	{
#define BLOCK_PROCESS_FUNC(x) case BlockType::x: return #x;
#include "BlockTypeList.hpp"
#undef BLOCK_PROCESS_FUNC
	case BlockType::NumBlockTypes: return "num block types";
	case BlockType::Unknown: return "unknown";
	};

	return "invalid";
}

// If this changed, GLSL code must be changed too!
enum class Direction : uint8_t
{
	Up,
	Down,
	North,
	South,
	NorthEast,
	SouthEast,
	NorthWest,
	SouthWest,
};

} // namespace HexGPU
