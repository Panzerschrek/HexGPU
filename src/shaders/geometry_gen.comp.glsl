#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

// If this chanhed, vertex attributes specification in C++ code must be chaned too!
struct WorldVertex
{
	i16vec4 pos;
	u8vec4 color;
	u8vec4 reserved[3];
};

struct Quad
{
	WorldVertex vertices[4];
};

// Command struct as it is defined in C.
struct VkDrawIndexedIndirectCommand
{
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int vertexOffset;
	uint firstInstance;
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

layout(binding= 2, std430) buffer chunk_data_buffer
{
	uint8_t chunk_data[];
};

const int c_indices_per_quad= 6;

const int c_chunk_width_log2= 4;

int ChunkBlockAddress(int x, int y, int z)
{
	return z + (y << c_chunk_width_log2) + (x << (c_chunk_width_log2 * 2));
}

void main()
{
	uvec3 invocation= gl_GlobalInvocationID;

	int chunk_offset_x= 0;
	int chunk_offset_y= 0;
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
	if( chunk_data[ block_address_in_chunk ] != chunk_data[ block_address_in_chunk + 1 ] )
	{
		// Add two hexagon quads.
		uint prev_index_count= atomicAdd(command.indexCount, c_indices_per_quad * 2);
		uint quad_index= prev_index_count / 6;

		{
			Quad quad;

			quad.vertices[0].pos= i16vec4(int16_t(base_x), int16_t(base_y), int16_t(z + 1), 0.0);
			quad.vertices[1].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y), int16_t(z + 1), 0.0);
			quad.vertices[2].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z + 1), 0.0);
			quad.vertices[3].pos= i16vec4(int16_t(base_x - 1), int16_t(base_y + 1), int16_t(z + 1), 0.0);

			quad.vertices[0].color[0]= uint8_t((invocation.x + 1) * 32);
			quad.vertices[0].color[1]= uint8_t((invocation.y + 1) * 32);
			quad.vertices[0].color[2]= uint8_t(0);
			quad.vertices[1].color[0]= uint8_t(invocation.x * 32);
			quad.vertices[1].color[1]= uint8_t((invocation.y + 1) * 32);
			quad.vertices[1].color[2]= uint8_t(0);
			quad.vertices[2].color[0]= uint8_t(invocation.x * 32);
			quad.vertices[2].color[1]= uint8_t(invocation.y * 32);
			quad.vertices[2].color[2]= uint8_t(0);
			quad.vertices[3].color[0]= uint8_t((invocation.x + 1) * 32);
			quad.vertices[3].color[1]= uint8_t(invocation.y * 32);
			quad.vertices[3].color[2]= uint8_t(0);

			quads[quad_index]= quad;
		}
		{
			Quad quad;

			quad.vertices[0].pos= i16vec4(int16_t(base_x - 1), int16_t(base_y + 1), int16_t(z + 1), 0.0);
			quad.vertices[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z + 1), 0.0);
			quad.vertices[2].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z + 1), 0.0);
			quad.vertices[3].pos= i16vec4(int16_t(base_x), int16_t(base_y + 2), int16_t(z + 1), 0.0);

			quad.vertices[0].color[0]= uint8_t((invocation.x + 1) * 32);
			quad.vertices[0].color[1]= uint8_t((invocation.y + 1) * 32);
			quad.vertices[0].color[2]= uint8_t(0);
			quad.vertices[1].color[0]= uint8_t(invocation.x * 32);
			quad.vertices[1].color[1]= uint8_t((invocation.y + 1) * 32);
			quad.vertices[1].color[2]= uint8_t(0);
			quad.vertices[2].color[0]= uint8_t(invocation.x * 32);
			quad.vertices[2].color[1]= uint8_t(invocation.y * 32);
			quad.vertices[2].color[2]= uint8_t(0);
			quad.vertices[3].color[0]= uint8_t((invocation.x + 1) * 32);
			quad.vertices[3].color[1]= uint8_t(invocation.y * 32);
			quad.vertices[3].color[2]= uint8_t(0);

			quads[quad_index + 1]= quad;
		}
	}

	if( chunk_data[ block_address_in_chunk ] != chunk_data[ block_address_in_chunk + (1 << (c_chunk_width_log2 * 2)) ] )
	{
		// Add north quad.
		uint prev_index_count= atomicAdd(command.indexCount, c_indices_per_quad);
		uint quad_index= prev_index_count / 6;

		Quad quad;

		quad.vertices[0].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z), 0.0);
		quad.vertices[1].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		quad.vertices[2].pos= i16vec4(int16_t(base_x), int16_t(base_y + 2), int16_t(z + 1), 0.0);
		quad.vertices[3].pos= i16vec4(int16_t(base_x), int16_t(base_y + 2), int16_t(z), 0.0);

		quad.vertices[0].color[0]= uint8_t((invocation.x + 1) * 32);
		quad.vertices[0].color[1]= uint8_t((invocation.y + 1) * 32);
		quad.vertices[0].color[2]= uint8_t(0);
		quad.vertices[1].color[0]= uint8_t(invocation.x * 32);
		quad.vertices[1].color[1]= uint8_t((invocation.y + 1) * 32);
		quad.vertices[1].color[2]= uint8_t(0);
		quad.vertices[2].color[0]= uint8_t(invocation.x * 32);
		quad.vertices[2].color[1]= uint8_t(invocation.y * 32);
		quad.vertices[2].color[2]= uint8_t(0);
		quad.vertices[3].color[0]= uint8_t((invocation.x + 1) * 32);
		quad.vertices[3].color[1]= uint8_t(invocation.y * 32);
		quad.vertices[3].color[2]= uint8_t(0);

		quads[quad_index]= quad;
	}
}
