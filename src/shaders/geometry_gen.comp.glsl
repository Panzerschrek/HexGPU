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

// If this changed, vertex attributes specification in C++ code must be chaned too!
struct WorldVertex
{
	i16vec4 pos;
	i16vec4 tex_coord; // Also stores texture index and light
};

struct Quad
{
	WorldVertex vertices[4];
};

layout(binding= 0, std430) buffer vertices_buffer
{
	// Populate here quads list.
	Quad quads[];
};

layout(binding= 1, std430) readonly buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};

layout(binding= 2, std430) readonly buffer chunk_light_buffer
{
	uint8_t light_buffer[];
};

layout(binding= 3, std430) buffer chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[];
};

layout(binding= 4, std430) readonly buffer chunks_auxiliar_data_buffer
{
	uint8_t chunks_auxiliar_data[];
};

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 chunk_position;
	ivec2 chunk_global_position;
};

// Use scale slightly less or equal to 272.
// Use slightly different scale for different block sides in order to make lightling less flat.
int16_t RepackAndScaleLight(uint8_t light_packed, int scale)
{
	int fire_light= int(light_packed) & c_fire_light_mask;
	int sky_light = int(light_packed) >> c_sky_light_shift;

	// Max possible result value is 255. (15 * 272 >> 4) is equal to maximum byte value 255.
	int fire_light_scaled= (fire_light * scale) >> 4;
	int sky_light_scaled = (sky_light  * scale) >> 4;

	return int16_t(fire_light_scaled | (sky_light_scaled << 8));
}

void main()
{
	// Generate quads geoemtry.
	// This code must mutch code in geometry size calculation code!

	int chunk_index= chunk_position.x + chunk_position.y * world_size_chunks.x;

	const uint quads_offset= chunk_draw_info[chunk_index].first_quad;

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

	// Perform calculations in integers - for simplicity.
	// Hexagon grid vertices are nicely aligned to scaled square grid.
	int base_x= 3 * ((chunk_global_position.x << c_chunk_width_log2) + int(invocation.x));
	int base_y= 2 * ((chunk_global_position.y << c_chunk_width_log2) + int(invocation.y)) - (block_x & 1) + 1;

	int base_tc_x= 4 * ((chunk_global_position.x << c_chunk_width_log2) + int(invocation.x));

	if(optical_density != optical_density_up)
	{
		// Add two hexagon quads.

		// Calculate hexagon vertices.
		WorldVertex v[6];

		v[0].pos= i16vec4(int16_t(base_x + 1), int16_t(base_y + 0), int16_t(z + 1), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 0), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 4), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x + 0), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[4].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		v[5].pos= i16vec4(int16_t(base_x + 1), int16_t(base_y + 2), int16_t(z + 1), 0.0);

		int16_t tex_index=
			optical_density < optical_density_up
				? c_block_texture_table[int(block_value)].r
				: c_block_texture_table[int(block_value_up)].g;

		ivec2 tc_base= ivec2(base_x, base_y);

		int16_t light= RepackAndScaleLight(light_buffer[optical_density > optical_density_up ? block_address : block_address_up], 272);

		v[0].tex_coord= i16vec4(int16_t(tc_base.x + 1), int16_t(tc_base.y + 0), tex_index, light);
		v[1].tex_coord= i16vec4(int16_t(tc_base.x + 3), int16_t(tc_base.y + 0), tex_index, light);
		v[2].tex_coord= i16vec4(int16_t(tc_base.x + 4), int16_t(tc_base.y + 1), tex_index, light);
		v[3].tex_coord= i16vec4(int16_t(tc_base.x + 0), int16_t(tc_base.y + 1), tex_index, light);
		v[4].tex_coord= i16vec4(int16_t(tc_base.x + 3), int16_t(tc_base.y + 2), tex_index, light);
		v[5].tex_coord= i16vec4(int16_t(tc_base.x + 1), int16_t(tc_base.y + 2), tex_index, light);

		// Create quads from hexagon vertices. Two vertices are shared.
		Quad quad_south, quad_north;
		quad_south.vertices[1]= v[1];
		quad_south.vertices[3]= v[3];
		quad_north.vertices[1]= v[2];
		quad_north.vertices[3]= v[5];
		if(optical_density < optical_density_up)
		{
			quad_south.vertices[0]= v[0];
			quad_south.vertices[2]= v[2];
			quad_north.vertices[0]= v[3];
			quad_north.vertices[2]= v[4];
		}
		else
		{
			quad_south.vertices[0]= v[2];
			quad_south.vertices[2]= v[0];
			quad_north.vertices[0]= v[4];
			quad_north.vertices[2]= v[3];
		}

		uint quad_index= quads_offset + atomicAdd(chunk_draw_info[chunk_index].num_quads, 2);
		quads[quad_index]= quad_south;
		quads[quad_index + 1]= quad_north;
	}

	if(optical_density != optical_density_north)
	{
		// Add north quad.
		WorldVertex v[4];

		v[0].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 2), int16_t(z + 0), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 1), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x + 1), int16_t(base_y + 2), int16_t(z + 0), 0.0);

		int16_t tex_index= c_block_texture_table[optical_density < optical_density_north ? int(block_value) : int(block_value_north)].b;

		ivec2 tc_base= ivec2(base_tc_x, z * 2);

		int16_t light= RepackAndScaleLight(light_buffer[optical_density > optical_density_north ? block_address : block_address_north], 267);

		v[0].tex_coord= i16vec4(int16_t(tc_base.x + 2), int16_t(tc_base.y + 0), tex_index, light);
		v[1].tex_coord= i16vec4(int16_t(tc_base.x + 2), int16_t(tc_base.y + 2), tex_index, light);
		v[2].tex_coord= i16vec4(int16_t(tc_base.x + 0), int16_t(tc_base.y + 2), tex_index, light);
		v[3].tex_coord= i16vec4(int16_t(tc_base.x + 0), int16_t(tc_base.y + 0), tex_index, light);

		Quad quad;
		quad.vertices[1]= v[1];
		quad.vertices[3]= v[3];
		if(optical_density < optical_density_north)
		{
			quad.vertices[0]= v[2];
			quad.vertices[2]= v[0];
		}
		else
		{
			quad.vertices[0]= v[0];
			quad.vertices[2]= v[2];
		}

		uint quad_index= quads_offset + atomicAdd(chunk_draw_info[chunk_index].num_quads, 1);
		quads[quad_index]= quad;
	}

	if(optical_density != optical_density_north_east)
	{
		// Add north-east quad.
		WorldVertex v[4];

		v[0].pos= i16vec4(int16_t(base_x + 4), int16_t(base_y + 1), int16_t(z + 0), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 4), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 2), int16_t(z + 0), 0.0);

		int16_t tex_index= c_block_texture_table[optical_density < optical_density_north_east ? int(block_value) : int(block_value_north_east)].b;

		ivec2 tc_base= ivec2(base_tc_x, z * 2);

		int16_t light= RepackAndScaleLight(light_buffer[optical_density > optical_density_north_east ? block_address : block_address_north_east], 262);

		v[0].tex_coord= i16vec4(int16_t(tc_base.x + 4), int16_t(tc_base.y + 0), tex_index, light);
		v[1].tex_coord= i16vec4(int16_t(tc_base.x + 4), int16_t(tc_base.y + 2), tex_index, light);
		v[2].tex_coord= i16vec4(int16_t(tc_base.x + 2), int16_t(tc_base.y + 2), tex_index, light);
		v[3].tex_coord= i16vec4(int16_t(tc_base.x + 2), int16_t(tc_base.y + 0), tex_index, light);

		Quad quad;
		quad.vertices[1]= v[1];
		quad.vertices[3]= v[3];
		if(optical_density < optical_density_north_east)
		{
			quad.vertices[0]= v[2];
			quad.vertices[2]= v[0];
		}
		else
		{
			quad.vertices[0]= v[0];
			quad.vertices[2]= v[2];
		}

		uint quad_index= quads_offset + atomicAdd(chunk_draw_info[chunk_index].num_quads, 1);
		quads[quad_index]= quad;
	}

	if(optical_density != optical_density_south_east)
	{
		WorldVertex v[4];

		v[0].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 0), int16_t(z + 0), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 0), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 4), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x + 4), int16_t(base_y + 1), int16_t(z + 0), 0.0);

		int16_t tex_index= c_block_texture_table[optical_density < optical_density_south_east ? int(block_value) : int(block_value_south_east)].b;

		ivec2 tc_base= ivec2(base_tc_x, z * 2);

		int16_t light= RepackAndScaleLight(light_buffer[optical_density > optical_density_south_east ? block_address : block_address_south_east], 257);

		v[0].tex_coord= i16vec4(int16_t(tc_base.x + 2), int16_t(tc_base.y + 0), tex_index, light);
		v[1].tex_coord= i16vec4(int16_t(tc_base.x + 2), int16_t(tc_base.y + 2), tex_index, light);
		v[2].tex_coord= i16vec4(int16_t(tc_base.x + 4), int16_t(tc_base.y + 2), tex_index, light);
		v[3].tex_coord= i16vec4(int16_t(tc_base.x + 4), int16_t(tc_base.y + 0), tex_index, light);

		Quad quad;
		quad.vertices[1]= v[1];
		quad.vertices[3]= v[3];
		if(optical_density < optical_density_south_east)
		{
			quad.vertices[0]= v[2];
			quad.vertices[2]= v[0];
		}
		else
		{
			quad.vertices[0]= v[0];
			quad.vertices[2]= v[2];
		}

		// Add south-east quad.
		uint quad_index= quads_offset + atomicAdd(chunk_draw_info[chunk_index].num_quads, 1);
		quads[quad_index]= quad;
	}

	if(block_value == c_block_type_water && block_value_up != c_block_type_water)
	{
		// Add two water hexagon quads.

		// Calculate average water level for each vertex.
		// If block above adjacent is water - use maximum water level.
		// If adjacent block if air - use minimum water level.
		// Doing so we ensure that water surface is almost perfectly smooth.

		int side_y_base= block_y + ((block_x + 1) & 1);
		int west_x_clamped= max(block_x - 1, 0);

		int adjacent_blocks[6]= int[6](
			block_address_north,
			block_address_north_east,
			block_address_south_east,
			// south
			GetBlockFullAddress(ivec3(block_x, max(block_y - 1, 0), z), world_size_chunks),
			// south-west
			GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 1, max_world_coord.y)), z), world_size_chunks),
			// north-west
			GetBlockFullAddress(ivec3(west_x_clamped, max(0, min(side_y_base - 0, max_world_coord.y)), z), world_size_chunks) );

		int water_level= int(chunks_auxiliar_data[block_address]);

		bool upper_block_is_water[6]= bool[6](false, false, false, false, false, false);
		bool adjacent_block_is_air[6]= bool[6](false, false, false, false, false, false);
		int vertex_water_level[6]= int[6](water_level, water_level, water_level, water_level, water_level, water_level);
		int vertex_water_block_count[6]= int[6](1, 1, 1, 1, 1, 1);

		for(int i= 0; i < 6; ++i) // For adjacent blocks
		{
			int adjacent_block_address= adjacent_blocks[i];

			const int c_next_vertex_index_table[6]= int[6](1, 2, 3, 4, 5, 0);

			int v0= i;
			int v1= c_next_vertex_index_table[i];

			if( z < c_chunk_height - 1 && chunks_data[adjacent_block_address + 1] == c_block_type_water)
			{
				upper_block_is_water[v0]= true;
				upper_block_is_water[v1]= true;
			}
			else
			{
				uint8_t adjacent_block_type= chunks_data[adjacent_block_address];
				if(adjacent_block_type == c_block_type_air)
				{
					adjacent_block_is_air[v0]= true;
					adjacent_block_is_air[v1]= true;
				}
				else if(adjacent_block_type == c_block_type_water)
				{
					int level= int(chunks_auxiliar_data[adjacent_block_address]);
					vertex_water_level[v0]+= level;
					vertex_water_level[v1]+= level;
					++vertex_water_block_count[v0];
					++vertex_water_block_count[v1];
				}
			}
		}

		const int z_shift= 8;
		for(int i= 0; i < 6; ++i) // Calculate vertex water level.
		{
			if(upper_block_is_water[i])
				vertex_water_level[i]= ((z + 1) << z_shift) + 1;
			else if(adjacent_block_is_air[i])
				vertex_water_level[i]= (z << z_shift) + 1;
			else
				vertex_water_level[i]= (z << z_shift) + (vertex_water_level[i] / vertex_water_block_count[i]);
		}

		// Calculate hexagon vertices.
		WorldVertex v[6];
		v[0].pos= i16vec4(int16_t(base_x + 1), int16_t(base_y + 0), int16_t(vertex_water_level[4]), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 0), int16_t(vertex_water_level[3]), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 4), int16_t(base_y + 1), int16_t(vertex_water_level[2]), 0.0);
		v[3].pos= i16vec4(int16_t(base_x + 0), int16_t(base_y + 1), int16_t(vertex_water_level[5]), 0.0);
		v[4].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 2), int16_t(vertex_water_level[1]), 0.0);
		v[5].pos= i16vec4(int16_t(base_x + 1), int16_t(base_y + 2), int16_t(vertex_water_level[0]), 0.0);

		const uint tex_index= uint(c_block_texture_table[uint(c_block_type_water)].x);

		ivec2 tc_base= ivec2(base_x, base_y);

		// Use light of this block.
		int16_t light= RepackAndScaleLight(light_buffer[block_address], 272);

		v[0].tex_coord= i16vec4(int16_t(tc_base.x + 1), int16_t(tc_base.y + 0), tex_index, light);
		v[1].tex_coord= i16vec4(int16_t(tc_base.x + 3), int16_t(tc_base.y + 0), tex_index, light);
		v[2].tex_coord= i16vec4(int16_t(tc_base.x + 4), int16_t(tc_base.y + 1), tex_index, light);
		v[3].tex_coord= i16vec4(int16_t(tc_base.x + 0), int16_t(tc_base.y + 1), tex_index, light);
		v[4].tex_coord= i16vec4(int16_t(tc_base.x + 3), int16_t(tc_base.y + 2), tex_index, light);
		v[5].tex_coord= i16vec4(int16_t(tc_base.x + 1), int16_t(tc_base.y + 2), tex_index, light);

		// Create quads from hexagon vertices. Two vertices are shared.
		Quad quad_south, quad_north;

		quad_south.vertices[0]= v[0];
		quad_south.vertices[1]= v[1];
		quad_south.vertices[2]= v[2];
		quad_south.vertices[3]= v[3];

		quad_north.vertices[0]= v[3];
		quad_north.vertices[1]= v[2];
		quad_north.vertices[2]= v[4];
		quad_north.vertices[3]= v[5];

		uint quad_index= chunk_draw_info[chunk_index].first_water_quad + atomicAdd(chunk_draw_info[chunk_index].num_water_quads, 2);
		quads[quad_index]= quad_south;
		quads[quad_index + 1]= quad_north;
	}

	if(block_value == c_block_type_fire)
	{
		// Add fire quads.
		uint quad_index= chunk_draw_info[chunk_index].first_fire_quad + atomicAdd(chunk_draw_info[chunk_index].num_fire_quads, 3);

		const int16_t light= int16_t(0); // No need to set light for fire.
		int16_t tex_index= int16_t(0);

		WorldVertex center_vertices[2];
		center_vertices[0].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 1), int16_t(z + 0), 0);
		center_vertices[1].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 1), int16_t(z + 1), 0);
		center_vertices[0].tex_coord= i16vec4(int16_t(0), int16_t(0), tex_index, light);
		center_vertices[1].tex_coord= i16vec4(int16_t(0), int16_t(2), tex_index, light);

		{
			WorldVertex v[2];

			v[0].pos= i16vec4(int16_t(base_x), int16_t(base_y + 1), int16_t(z + 1), 0);
			v[1].pos= i16vec4(int16_t(base_x), int16_t(base_y + 1), int16_t(z + 0), 0);
			v[0].tex_coord= i16vec4(int16_t(2), int16_t(2), tex_index, light);
			v[1].tex_coord= i16vec4(int16_t(2), int16_t(0), tex_index, light);

			Quad quad;
			quad.vertices[0]= v[0];
			quad.vertices[1]= v[1];
			quad.vertices[2]= center_vertices[0];
			quad.vertices[3]= center_vertices[1];

			quads[quad_index]= quad;
		}
		{
			WorldVertex v[2];

			v[0].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 2), int16_t(z + 1), 0);
			v[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 2), int16_t(z + 0), 0);
			v[0].tex_coord= i16vec4(int16_t(2), int16_t(2), tex_index, light);
			v[1].tex_coord= i16vec4(int16_t(2), int16_t(0), tex_index, light);

			Quad quad;
			quad.vertices[0]= v[0];
			quad.vertices[1]= v[1];
			quad.vertices[2]= center_vertices[0];
			quad.vertices[3]= center_vertices[1];

			quads[quad_index + 1]= quad;
		}
		{
			WorldVertex v[2];

			v[0].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y ), int16_t(z + 1), 0);
			v[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y ), int16_t(z + 0), 0);
			v[0].tex_coord= i16vec4(int16_t(2), int16_t(2), tex_index, light);
			v[1].tex_coord= i16vec4(int16_t(2), int16_t(0), tex_index, light);

			Quad quad;
			quad.vertices[0]= v[0];
			quad.vertices[1]= v[1];
			quad.vertices[2]= center_vertices[0];
			quad.vertices[3]= center_vertices[1];

			quads[quad_index + 2]= quad;
		}
	}
}
