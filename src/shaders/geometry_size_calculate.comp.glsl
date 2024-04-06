#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/chunk_draw_info.glsl"
#include "inc/hex_funcs.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 4, local_size_y = 4, local_size_z= 8) in;

layout(binding= 0, std430) readonly buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};

layout(binding= 1, std430) buffer chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[];
};

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 chunk_position;
	ivec2 chunk_global_position;
};

void main()
{
	// Calculate only number of result quads.
	// This code must mutch code in geometry generation code!

	int chunk_index= chunk_position.x + chunk_position.y * world_size_chunks.x;

	uvec3 invocation= gl_GlobalInvocationID;

	int block_x= (chunk_position.x << c_chunk_width_log2) + int(invocation.x);
	int block_y= (chunk_position.y << c_chunk_width_log2) + int(invocation.y);
	int z= int(invocation.z);

	ivec2 max_world_coord= GetMaxWorldCoord(world_size_chunks);

	int east_x_clamped= min(block_x + 1, max_world_coord.x);
	int east_y_base= block_y + ((block_x + 1) & 1);

	// TODO - optimize this. Reuse calculations in the same chunk.
	int block_address= GetBlockFullAddress(ivec3(block_x, block_y, z), world_size_chunks);
	int block_address_up= GetBlockFullAddress(ivec3(block_x, block_y, min(z + 1, c_chunk_height - 1)), world_size_chunks);
	int block_address_north= GetBlockFullAddress(ivec3(block_x, min(block_y + 1, max_world_coord.y), z), world_size_chunks);
	int block_address_north_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(east_y_base - 0, max_world_coord.y)), z), world_size_chunks);
	int block_address_south_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(east_y_base - 1, max_world_coord.y)), z), world_size_chunks);

	uint8_t block_value= chunks_data[block_address];
	uint8_t block_value_up= chunks_data[block_address_up];
	uint8_t block_value_north= chunks_data[block_address_north];
	uint8_t block_value_north_east= chunks_data[block_address_north_east];
	uint8_t block_value_south_east= chunks_data[block_address_south_east];

	uint8_t optical_density= c_block_optical_density_table[int(block_value)];
	uint8_t optical_density_up= c_block_optical_density_table[int(block_value_up)];
	uint8_t optical_density_north= c_block_optical_density_table[int(block_value_north)];
	uint8_t optical_density_north_east= c_block_optical_density_table[int(block_value_north_east)];
	uint8_t optical_density_south_east= c_block_optical_density_table[int(block_value_south_east)];

	if(optical_density != optical_density_up && block_value_up != c_block_type_snow)
	{
		// Add two hexagon quads.
		atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 2);
	}

	if(optical_density != optical_density_north)
	{
		// Add north quad.
		atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
	}

	if(optical_density != optical_density_north_east)
	{
		// Add north-east quad.
		atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
	}

	if(optical_density != optical_density_south_east)
	{
		// Add south-east quad.
		atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
	}

	if(block_value == c_block_type_grass || block_value == c_block_type_grass_yellow)
	{
		if(block_value_up != c_block_type_snow)
		{
			// Add tall grass quads. Each grass block has exactly two of them.
			atomicAdd(chunk_draw_info[chunk_index].new_grass_num_quads, 2);
		}
	}
	else if(block_value == c_block_type_snow)
	{
		// Add two snow quads.
		atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 2);

		if(z > 0 && c_block_optical_density_table[uint(chunks_data[block_address - 1])] != c_optical_density_solid)
		{
			// Add two lower snow quads.
			atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 2);
		}
	}
	else if(block_value == c_block_type_water)
	{
		if(block_value_up != c_block_type_water)
		{
			// Add two water hexagon quads.
			atomicAdd(chunk_draw_info[chunk_index].new_water_num_quads, 2);
		}

		if(z > 0)
		{
			uint8_t block_value_down= chunks_data[block_address - 1];
			if(block_value_down != c_block_type_water && c_block_optical_density_table[uint(block_value_down)] != c_optical_density_solid)
			{
				// Add two water bottom quads.
				atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 2);
			}
		}

		if(block_value_north != c_block_type_water && optical_density_north != c_optical_density_solid)
		{
			// Add north water side quad.
			atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
		}

		if(block_value_north_east != c_block_type_water && optical_density_north_east != c_optical_density_solid)
		{
			// Add north-east water side quad.
			atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
		}

		if(block_value_south_east != c_block_type_water && optical_density_south_east != c_optical_density_solid)
		{
			// Add south-east water side quad.
			atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
		}

		int side_y_base= block_y + ((block_x + 1) & 1);
		int west_x_clamped= max(block_x - 1, 0);

		int south_block_address= GetBlockFullAddress(ivec3(block_x, max(block_y - 1, 0), z), world_size_chunks);
		int south_west_block_address= GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 1, max_world_coord.y)), z), world_size_chunks);
		int north_west_block_address= GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 0, max_world_coord.y)), z), world_size_chunks);

		uint8_t block_value_south= chunks_data[south_block_address];
		if(block_value_south != c_block_type_water && c_block_optical_density_table[uint(block_value_south)] != c_optical_density_solid)
		{
			// Add south water side quad.
			atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
		}

		uint8_t block_value_south_west= chunks_data[south_west_block_address];
		if(block_value_south_west != c_block_type_water && c_block_optical_density_table[uint(block_value_south_west)] != c_optical_density_solid)
		{
			// Add south-west water side quad.
			atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
		}

		uint8_t block_value_north_west= chunks_data[north_west_block_address];
		if(block_value_north_west != c_block_type_water && c_block_optical_density_table[uint(block_value_north_west)] != c_optical_density_solid)
		{
			// Add north-west water side quad.
			atomicAdd(chunk_draw_info[chunk_index].new_num_quads, 1);
		}
	}
	else if(block_value == c_block_type_fire)
	{
		// Add fire quads.
		int total_quads= 0;

		if(z > 0 && c_block_flammability_table[uint(chunks_data[block_address - 1])] != uint8_t(0))
			total_quads+= 3;

		if(c_block_flammability_table[uint(block_value_up)] != uint8_t(0))
			total_quads+= 3;

		if(c_block_flammability_table[uint(block_value_north)] != uint8_t(0))
			++total_quads;

		if(c_block_flammability_table[uint(block_value_north_east)] != uint8_t(0))
			++total_quads;

		if(c_block_flammability_table[uint(block_value_south_east)] != uint8_t(0))
			++total_quads;

		int south_block_address= GetBlockFullAddress(ivec3(block_x, max(block_y - 1, 0), z), world_size_chunks);
		if(c_block_flammability_table[uint(chunks_data[south_block_address])] != uint8_t(0))
			++total_quads;

		int side_y_base= block_y + ((block_x + 1) & 1);
		int west_x_clamped= max(block_x - 1, 0);

		int south_west_block_address= GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 1, max_world_coord.y)), z), world_size_chunks);
		if(c_block_flammability_table[uint(chunks_data[south_west_block_address])] != uint8_t(0))
			++total_quads;

		int north_west_block_address= GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 0, max_world_coord.y)), z), world_size_chunks);
		if(c_block_flammability_table[uint(chunks_data[north_west_block_address])] != uint8_t(0))
			++total_quads;

		atomicAdd(chunk_draw_info[chunk_index].new_fire_num_quads, total_quads);
	}
}
