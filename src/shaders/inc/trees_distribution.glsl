// This file must match the same definitions in C++ code!

const uint c_tree_map_size[2]= uint[2](256, 256);

const uint c_tree_map_cell_size[2]= uint[2](4, 2);

const uint c_tree_map_cell_grid_size[2] = uint[2]
(
	c_tree_map_size[0] / c_tree_map_cell_size[0],
	c_tree_map_size[1] / c_tree_map_cell_size[1]
);

struct TreeMapCell
{
	uint sequential_index;
	u16vec2 coord;
};

struct TreeMap
{
	TreeMapCell cells[c_tree_map_cell_grid_size[0] * c_tree_map_cell_grid_size[1]];
};
