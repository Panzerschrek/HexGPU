#version 450

#extension GL_GOOGLE_include_directive : require
#include "inc/constants.glsl"
#include "inc/vulkan_structs.glsl"

#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

// If this changed, vertex attributes specification in C++ code must be chaned too!
struct WorldVertex
{
	i16vec4 pos;
	i16vec2 tex_coord;
	i16vec2 reserved;
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

layout(push_constant) uniform uniforms_block
{
	int chunk_position[2];
};

const int c_indices_per_quad= 6;

const int c_max_quads_per_chunk= 65536 / 4;

const int c_max_global_x= (c_chunk_matrix_size[0] << c_chunk_width_log2) - 1;
const int c_max_global_y= (c_chunk_matrix_size[1] << c_chunk_width_log2) - 1;

// Input coordinates must be properly clamped.
uint8_t FetchBlock(int global_x, int global_y, int z)
{
	int chunk_x= global_x >> c_chunk_width_log2;
	int chunk_y= global_y >> c_chunk_width_log2;
	int local_x= global_x & (c_chunk_width - 1);
	int local_y= global_y & (c_chunk_width - 1);

	int chunk_index= chunk_x + chunk_y * c_chunk_matrix_size[0];
	int chunk_data_offset= chunk_index * c_chunk_volume;

	int block_address= ChunkBlockAddress(local_x, local_y, z);

	return chunks_data[chunk_data_offset + block_address];
}

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

	// TODO - optimize this. Reuse calculations in the same chunk.
	uint8_t block_value= FetchBlock(block_global_x, block_global_y, z);
	uint8_t block_value_up= FetchBlock(block_global_x, block_global_y, z + 1); // Assume Z is never for the last layer of blocks.
	uint8_t block_value_north= FetchBlock(block_global_x, min(block_global_y + 1, c_max_global_y), z);

	int east_x_clamped= min(block_global_x + 1, c_max_global_x);
	int east_y_base= block_global_y + ((block_global_x + 1) & 1);
	uint8_t block_value_north_east= FetchBlock(east_x_clamped, max(0, min(east_y_base - 0, c_max_global_y)), z);
	uint8_t block_value_south_east= FetchBlock(east_x_clamped, max(0, min(east_y_base - 1, c_max_global_y)), z);

	// Perform calculations in integers - for simplicity.
	// Hexagon grid vertices are nicely aligned to scaled square grid.
	int base_x= 3 * block_global_x;
	int base_y= 2 * block_global_y - (block_global_x & 1) + 1;
	const int tex_scale= 1; // TODO - read block properties to determine texture scale.

	if( block_value != block_value_up )
	{
		// Add two hexagon quads.

		// Calculate hexagon vertices.
		WorldVertex v[6];

		v[0].pos= i16vec4(int16_t(base_x + 0), int16_t(base_y + 0), int16_t(z + 1), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 0), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x - 1), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[4].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		v[5].pos= i16vec4(int16_t(base_x + 0), int16_t(base_y + 2), int16_t(z + 1), 0.0);

		int tc_base_x= base_x * tex_scale;
		int tc_base_y= base_y * tex_scale;

		v[0].tex_coord= i16vec2(int16_t(tc_base_x + 0 * tex_scale), int16_t(tc_base_y + 0 * tex_scale));
		v[1].tex_coord= i16vec2(int16_t(tc_base_x + 2 * tex_scale), int16_t(tc_base_y + 0 * tex_scale));
		v[2].tex_coord= i16vec2(int16_t(tc_base_x + 3 * tex_scale), int16_t(tc_base_y + 1 * tex_scale));
		v[3].tex_coord= i16vec2(int16_t(tc_base_x - 1 * tex_scale), int16_t(tc_base_y + 1 * tex_scale));
		v[4].tex_coord= i16vec2(int16_t(tc_base_x + 2 * tex_scale), int16_t(tc_base_y + 2 * tex_scale));
		v[5].tex_coord= i16vec2(int16_t(tc_base_x + 0 * tex_scale), int16_t(tc_base_y + 2 * tex_scale));

		// Create quads from hexagon vertices. Two vertices are shared.
		Quad quad_south, quad_north;
		quad_south.vertices[1]= v[1];
		quad_south.vertices[3]= v[3];
		quad_north.vertices[1]= v[2];
		quad_north.vertices[3]= v[5];
		if( block_value > block_value_up )
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

	if( block_value != block_value_north )
	{
		// Add north quad.
		WorldVertex v[4];

		v[0].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z + 0), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 0), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x + 0), int16_t(base_y + 2), int16_t(z + 0), 0.0);

		int tc_base_x= base_x * tex_scale;
		int tc_base_z= z * (2 * tex_scale);

		v[0].tex_coord= i16vec2(int16_t(tc_base_x + 2 * tex_scale), int16_t(tc_base_z + 0 * tex_scale));
		v[1].tex_coord= i16vec2(int16_t(tc_base_x + 2 * tex_scale), int16_t(tc_base_z + 2 * tex_scale));
		v[2].tex_coord= i16vec2(int16_t(tc_base_x + 0 * tex_scale), int16_t(tc_base_z + 2 * tex_scale));
		v[3].tex_coord= i16vec2(int16_t(tc_base_x + 0 * tex_scale), int16_t(tc_base_z + 0 * tex_scale));

		Quad quad;
		quad.vertices[1]= v[1];
		quad.vertices[3]= v[3];
		if( block_value > block_value_north )
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

	if( block_value != block_value_north_east )
	{
		// Add north-east quad.
		WorldVertex v[4];

		v[0].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z + 0), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z + 0), 0.0);

		int tc_base_x= base_x * tex_scale;
		int tc_base_z= z * (2 * tex_scale);

		v[0].tex_coord= i16vec2(int16_t(tc_base_x + 2 * tex_scale), int16_t(tc_base_z + 0 * tex_scale));
		v[1].tex_coord= i16vec2(int16_t(tc_base_x + 2 * tex_scale), int16_t(tc_base_z + 2 * tex_scale));
		v[2].tex_coord= i16vec2(int16_t(tc_base_x + 0 * tex_scale), int16_t(tc_base_z + 2 * tex_scale));
		v[3].tex_coord= i16vec2(int16_t(tc_base_x + 0 * tex_scale), int16_t(tc_base_z + 0 * tex_scale));

		Quad quad;
		quad.vertices[1]= v[1];
		quad.vertices[3]= v[3];
		if( block_value > block_value_north_east )
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

	if( block_value != block_value_south_east )
	{
		WorldVertex v[4];

		v[0].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 0), int16_t(z + 0), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 0), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z + 0), 0.0);

		int tc_base_x= base_x * tex_scale;
		int tc_base_z= z * (2 * tex_scale);

		v[0].tex_coord= i16vec2(int16_t(tc_base_x + 2 * tex_scale), int16_t(tc_base_z + 0 * tex_scale));
		v[1].tex_coord= i16vec2(int16_t(tc_base_x + 2 * tex_scale), int16_t(tc_base_z + 2 * tex_scale));
		v[2].tex_coord= i16vec2(int16_t(tc_base_x + 0 * tex_scale), int16_t(tc_base_z + 2 * tex_scale));
		v[3].tex_coord= i16vec2(int16_t(tc_base_x + 0 * tex_scale), int16_t(tc_base_z + 0 * tex_scale));

		Quad quad;
		quad.vertices[1]= v[1];
		quad.vertices[3]= v[3];
		if( block_value > block_value_south_east )
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
