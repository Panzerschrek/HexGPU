#pragma once
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

} // namespace HexGPU
