#version 450

#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/chunk_gen_info.glsl"
#include "inc/constants.glsl"
#include "inc/structures.glsl"
#include "inc/trees_distribution.glsl"
#include "inc/world_gen_common.glsl"

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 chunk_position;
	ivec2 chunk_global_position;
	int seed;
};

layout(binding= 0, std430) buffer chunk_gen_info_buffer
{
	ChunkGenInfo chunk_gen_infos[];
};

layout(binding= 1, std430) buffer structure_descriptions_buffer
{
	StructureDescription structure_descriptions[];
};

layout(binding= 2, std430) buffer tree_map_buffer
{
	TreeMap tree_map;
};

void main()
{
	int chunk_index= chunk_position.x + chunk_position.y * world_size_chunks.x;
	chunk_gen_infos[chunk_index].num_structures= 0;

	const int c_cells_per_chunk[2]=
		int[2](int(c_chunk_width >> c_tree_map_cell_size_log2[0]), int(c_chunk_width >> c_tree_map_cell_size_log2[1]));

	// Add extra border to process trees in adjacent chunks.
	int tree_grid_cell_start_x= chunk_global_position.x * c_cells_per_chunk[0] - 1;
	int tree_grid_cell_start_y= chunk_global_position.y * c_cells_per_chunk[1] - 2;
	int tree_grid_cell_end_x= tree_grid_cell_start_x + c_cells_per_chunk[0] + 2;
	int tree_grid_cell_end_y= tree_grid_cell_start_y + c_cells_per_chunk[1] + 4;
	for(int cell_y= tree_grid_cell_start_y; cell_y < tree_grid_cell_end_y; ++cell_y)
	for(int cell_x= tree_grid_cell_start_x; cell_x < tree_grid_cell_end_x; ++cell_x)
	{
		int cell_index=
			(cell_x & int(c_tree_map_cell_grid_size[0] - 1)) +
			(cell_y & int(c_tree_map_cell_grid_size[1] - 1)) * int(c_tree_map_cell_grid_size[0]);

		TreeMapCell cell= tree_map.cells[cell_index];
		if(cell.sequential_index == 0)
			continue; // This cell contains no point.

		int tree_global_x= (cell_x << int(c_tree_map_cell_size_log2[0])) + int(cell.coord.x);
		int tree_global_y= (cell_y << int(c_tree_map_cell_size_log2[1])) + int(cell.coord.y);

		int z= GetGroundLevel(tree_global_x, tree_global_y, seed) + 1;
		if(z <= c_water_level + 1)
			continue;

		int tree_chunk_x= tree_global_x - (chunk_global_position.x << c_chunk_width_log2);
		int tree_chunk_y= tree_global_y - (chunk_global_position.y << c_chunk_width_log2);

		// Choose tree model based on point radius (greater radius - bigger the tree).
		uint structure_id= uint(cell.radius) <= 2u ? 1u : 0u;

		u8vec4 structure_size= structure_descriptions[structure_id].size;
		u8vec4 structure_center= structure_descriptions[structure_id].center;

		ivec2 min_xy= ivec2(tree_chunk_x - int(structure_center.x), tree_chunk_y - int(structure_center.y));
		// Adding extra 1 for "y" is important here to handle shifted columns.
		ivec2 max_xy= min_xy + ivec2(int(structure_size.x), int(structure_size.y) + 1);

		if( min_xy.x >= c_chunk_width || max_xy.x <= 0 || min_xy.y >= c_chunk_width || max_xy.y <= 0)
			continue; // This tree lies fully outside this chunk.


		ChunkStructureDescription chunk_structure;
		chunk_structure.min= i8vec4(i8vec2(min_xy), int8_t(z), int8_t(structure_id));
		chunk_structure.max= i8vec4(i8vec2(max_xy), int8_t(z + int(structure_size.z)), 0);

		chunk_gen_infos[chunk_index].structures[ chunk_gen_infos[chunk_index].num_structures ]= chunk_structure;
		++chunk_gen_infos[chunk_index].num_structures;
		if( chunk_gen_infos[chunk_index].num_structures >= c_max_chunk_structures)
			return;
	}
}
