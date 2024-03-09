#pragma once
#include <array>

namespace HexGPU
{

// Size in blocks.
constexpr std::array<uint32_t, 2> c_tree_map_size{256, 256};

// Each cell contains zero or one point.
constexpr std::array<uint32_t, 2> c_tree_map_cell_size{4, 2};

static_assert(c_tree_map_size[0] % c_tree_map_cell_size[0] == 0, "Wrong size");
static_assert(c_tree_map_size[1] % c_tree_map_cell_size[1] == 0, "Wrong size");

constexpr std::array<uint32_t, 2> c_tree_map_cell_grid_size
{
	c_tree_map_size[0] / c_tree_map_cell_size[0],
	c_tree_map_size[1] / c_tree_map_cell_size[1]
};

// This struct must match the same struct in GLSL code!
struct TreeMapCell
{
	uint32_t sequential_index= 0; // Index of this point. If 0 - this cell contains no point.
	uint16_t coord[2]{}; // Coordinates relative to tree map start.
};

using TreeMap= std::array<TreeMapCell, c_tree_map_cell_grid_size[0] * c_tree_map_cell_grid_size[1]>;

TreeMap GenTreeMap();

} // namespace HexGPU
