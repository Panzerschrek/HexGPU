#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/chunk_draw_info.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/vulkan_structs.glsl"

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

layout(binding= 1, std430) buffer draw_indirect_buffer
{
	// Populate here result number of indices.
	VkDrawIndexedIndirectCommand draw_commands[c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(binding= 2, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(binding= 3, std430) buffer chunk_light_buffer
{
	uint8_t light_buffer[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(binding= 4, std430) buffer chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(push_constant) uniform uniforms_block
{
	int chunk_position[2];
};

const int c_indices_per_quad= 6;

const int c_max_quads_per_chunk= 65536 / 4;

void main()
{
	int chunk_index= chunk_position[0] + chunk_position[1] * c_chunk_matrix_size[0];

	// For now use same capacity for quads of all chunks.
	// TODO - allocate memory for chunk quads on per-chunk basis, read here offset to allocated memory.
	const uint quads_offset= uint(c_max_quads_per_chunk * chunk_index);

	uvec3 invocation= gl_GlobalInvocationID;

	int block_global_x= (chunk_position[0] << c_chunk_width_log2) + int(invocation.x);
	int block_global_y= (chunk_position[1] << c_chunk_width_log2) + int(invocation.y);
	int z= int(invocation.z);

	int east_x_clamped= min(block_global_x + 1, c_max_global_x);
	int east_y_base= block_global_y + ((block_global_x + 1) & 1);

	// TODO - optimize this. Reuse calculations in the same chunk.
	int block_address= GetBlockFullAddress(ivec3(block_global_x, block_global_y, z));
	int block_address_up= GetBlockFullAddress(ivec3(block_global_x, block_global_y, z + 1)); // Assume Z is never for the last layer of blocks.
	int block_address_north= GetBlockFullAddress(ivec3(block_global_x, min(block_global_y + 1, c_max_global_y), z));
	int block_address_north_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(east_y_base - 0, c_max_global_y)), z));
	int block_address_south_east= GetBlockFullAddress(ivec3(east_x_clamped, max(0, min(east_y_base - 1, c_max_global_y)), z));

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
	int base_x= 3 * block_global_x;
	int base_y= 2 * block_global_y - (block_global_x & 1) + 1;
	const int tex_scale= 1; // TODO - read block properties to determine texture scale.

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

		int tc_base_x= base_x * tex_scale;
		int tc_base_y= base_y * tex_scale;

		int16_t tex_index=
			optical_density < optical_density_up
				? c_block_texture_table[int(block_value)].r
				: c_block_texture_table[int(block_value_up)].g;
		int16_t light= int16_t(light_buffer[optical_density > optical_density_up ? block_address : block_address_up]);

		v[0].tex_coord= i16vec4(int16_t(tc_base_x + 1 * tex_scale), int16_t(tc_base_y + 0 * tex_scale), tex_index, light);
		v[1].tex_coord= i16vec4(int16_t(tc_base_x + 3 * tex_scale), int16_t(tc_base_y + 0 * tex_scale), tex_index, light);
		v[2].tex_coord= i16vec4(int16_t(tc_base_x + 4 * tex_scale), int16_t(tc_base_y + 1 * tex_scale), tex_index, light);
		v[3].tex_coord= i16vec4(int16_t(tc_base_x + 0 * tex_scale), int16_t(tc_base_y + 1 * tex_scale), tex_index, light);
		v[4].tex_coord= i16vec4(int16_t(tc_base_x + 3 * tex_scale), int16_t(tc_base_y + 2 * tex_scale), tex_index, light);
		v[5].tex_coord= i16vec4(int16_t(tc_base_x + 1 * tex_scale), int16_t(tc_base_y + 2 * tex_scale), tex_index, light);

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

		uint prev_index_count= atomicAdd(draw_commands[chunk_index].indexCount, c_indices_per_quad * 2);
		uint quad_index= quads_offset + prev_index_count / 6;
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

		int tc_base_x= base_x * tex_scale;
		int tc_base_z= z * (2 * tex_scale);

		int16_t tex_index= c_block_texture_table[optical_density < optical_density_north ? int(block_value) : int(block_value_north)].b;
		int16_t light= int16_t(light_buffer[optical_density > optical_density_north ? block_address : block_address_north]);

		v[0].tex_coord= i16vec4(int16_t(tc_base_x + 3 * tex_scale), int16_t(tc_base_z + 0 * tex_scale), tex_index, light);
		v[1].tex_coord= i16vec4(int16_t(tc_base_x + 3 * tex_scale), int16_t(tc_base_z + 2 * tex_scale), tex_index, light);
		v[2].tex_coord= i16vec4(int16_t(tc_base_x + 1 * tex_scale), int16_t(tc_base_z + 2 * tex_scale), tex_index, light);
		v[3].tex_coord= i16vec4(int16_t(tc_base_x + 1 * tex_scale), int16_t(tc_base_z + 0 * tex_scale), tex_index, light);

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

		uint prev_index_count= atomicAdd(draw_commands[chunk_index].indexCount, c_indices_per_quad);
		uint quad_index= quads_offset + prev_index_count / 6;
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

		int tc_base_x= base_x * tex_scale;
		int tc_base_z= z * (2 * tex_scale);

		int16_t tex_index= c_block_texture_table[optical_density < optical_density_north_east ? int(block_value) : int(block_value_north_east)].b;
		int16_t light= int16_t(light_buffer[optical_density > optical_density_north_east ? block_address : block_address_north_east]);

		v[0].tex_coord= i16vec4(int16_t(tc_base_x + 3 * tex_scale), int16_t(tc_base_z + 0 * tex_scale), tex_index, light);
		v[1].tex_coord= i16vec4(int16_t(tc_base_x + 3 * tex_scale), int16_t(tc_base_z + 2 * tex_scale), tex_index, light);
		v[2].tex_coord= i16vec4(int16_t(tc_base_x + 1 * tex_scale), int16_t(tc_base_z + 2 * tex_scale), tex_index, light);
		v[3].tex_coord= i16vec4(int16_t(tc_base_x + 1 * tex_scale), int16_t(tc_base_z + 0 * tex_scale), tex_index, light);

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

		uint prev_index_count= atomicAdd(draw_commands[chunk_index].indexCount, c_indices_per_quad);
		uint quad_index= quads_offset + prev_index_count / 6;
		quads[quad_index]= quad;
	}

	if(optical_density != optical_density_south_east)
	{
		WorldVertex v[4];

		v[0].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 0), int16_t(z + 0), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 0), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 4), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x + 4), int16_t(base_y + 1), int16_t(z + 0), 0.0);

		int tc_base_x= base_x * tex_scale;
		int tc_base_z= z * (2 * tex_scale);

		int16_t tex_index= c_block_texture_table[optical_density < optical_density_south_east ? int(block_value) : int(block_value_south_east)].b;
		int16_t light= int16_t(light_buffer[optical_density > optical_density_south_east ? block_address : block_address_south_east]);

		v[0].tex_coord= i16vec4(int16_t(tc_base_x + 3 * tex_scale), int16_t(tc_base_z + 0 * tex_scale), tex_index, light);
		v[1].tex_coord= i16vec4(int16_t(tc_base_x + 3 * tex_scale), int16_t(tc_base_z + 2 * tex_scale), tex_index, light);
		v[2].tex_coord= i16vec4(int16_t(tc_base_x + 1 * tex_scale), int16_t(tc_base_z + 2 * tex_scale), tex_index, light);
		v[3].tex_coord= i16vec4(int16_t(tc_base_x + 1 * tex_scale), int16_t(tc_base_z + 0 * tex_scale), tex_index, light);

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
		uint prev_index_count= atomicAdd(draw_commands[chunk_index].indexCount, c_indices_per_quad);
		uint quad_index= quads_offset + prev_index_count / 6;
		quads[quad_index]= quad;
	}
}
