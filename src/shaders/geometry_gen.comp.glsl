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
	VkDrawIndexedIndirectCommand command;
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

void main()
{
	uvec3 invocation= gl_GlobalInvocationID;

	int chunk_offset_x= chunk_position[0] << c_chunk_width_log2;
	int chunk_offset_y= chunk_position[1] << c_chunk_width_log2;
	int block_local_x= int(invocation.x + 1); // Skip borders.
	int block_local_y= int(invocation.y + 1); // Skip borders.
	int z= int(invocation.z + 1); // Skip borders.
	int block_global_x= chunk_offset_x + block_local_x;
	int block_global_y= chunk_offset_y + block_local_y;

	// Perform calculations in integers - for simplicity.
	// Hexagon grid vertices are nicely aligned to scaled square grid.
	int base_x= 3 * block_global_x;
	int base_y= 2 * block_global_y - (block_global_x & 1) + 1;

	int block_address_in_chunk= ChunkBlockAddress( block_local_x, block_local_y, z );
	int block_address_up= block_address_in_chunk + 1;
	int block_address_north= block_address_in_chunk + (1 << c_chunk_height_log2);
	int block_address_north_east= block_address_in_chunk + (((block_local_x + 1) & 1) << (c_chunk_height_log2)) + (1 <<(c_chunk_width_log2 + c_chunk_height_log2));
	int block_address_south_east= block_address_north_east - (1 << c_chunk_height_log2);

	int chunk_data_offset= (chunk_position[0] + chunk_position[1] * c_chunk_matrix_size[0]) * c_chunk_volume;

	uint8_t block_value= chunks_data[ chunk_data_offset + block_address_in_chunk ];
	uint8_t block_value_up= chunks_data[ chunk_data_offset + block_address_up ];
	uint8_t block_value_north= chunks_data[ chunk_data_offset + block_address_north ];
	uint8_t block_value_north_east= chunks_data[ chunk_data_offset + block_address_north_east ];
	uint8_t block_value_south_east= chunks_data[ chunk_data_offset + block_address_south_east ];

	const int tex_scale= 1; // TODO - read block properties to determine texture scale.

	if( block_value != block_value_up )
	{
		// Add two hexagon quads.
		uint prev_index_count= atomicAdd(command.indexCount, c_indices_per_quad * 2);
		uint quad_index= prev_index_count / 6;

		int tc_base_x= base_x * tex_scale;
		int tc_base_y= base_y * tex_scale;

		// Calculate hexagon vertices.
		WorldVertex v[6];

		v[0].pos= i16vec4(int16_t(base_x + 0), int16_t(base_y + 0), int16_t(z + 1), 0.0);
		v[1].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 0), int16_t(z + 1), 0.0);
		v[2].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[3].pos= i16vec4(int16_t(base_x - 1), int16_t(base_y + 1), int16_t(z + 1), 0.0);
		v[4].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		v[5].pos= i16vec4(int16_t(base_x + 0), int16_t(base_y + 2), int16_t(z + 1), 0.0);

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

		quads[quad_index]= quad_south;
		quads[quad_index + 1]= quad_north;
	}

	if( block_value != block_value_north )
	{
		// Add north quad.
		uint prev_index_count= atomicAdd(command.indexCount, c_indices_per_quad);
		uint quad_index= prev_index_count / 6;

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

		quads[quad_index]= quad;
	}

	if( block_value != block_value_north_east )
	{
		// Add north-east quad.
		uint prev_index_count= atomicAdd(command.indexCount, c_indices_per_quad);
		uint quad_index= prev_index_count / 6;

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

		quads[quad_index]= quad;
	}

	if( block_value != block_value_south_east )
	{
		// Add south-east quad.
		uint prev_index_count= atomicAdd(command.indexCount, c_indices_per_quad);
		uint quad_index= prev_index_count / 6;

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

		quads[quad_index]= quad;
	}
}
