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

layout(binding= 1, std430) buffer chunk_data_buffer
{
	uint8_t chunk_data[];
};

const int c_indices_per_quad= 6;

int GetBlockZ( int x, int y )
{
	return (x + y) >> 2;
}

void main()
{
	uvec3 invocation= gl_GlobalInvocationID;

	int chunk_offset_x= 0;
	int chunk_offset_y= 0;
	int block_local_x= int(invocation.x);
	int block_local_y= int(invocation.y);
	int block_global_x= chunk_offset_x + block_local_x;
	int block_global_y= chunk_offset_y + block_local_y;
	int z= GetBlockZ(block_global_x, block_global_y);

	// Perform calculations in integers - for simplicity.
	// Hexagon grid vertices are nicely aligned to scaled square grid.
	int base_x= 3 * block_global_x;
	int base_y= 2 * block_global_y - (block_global_x & 1) + 1;

	{
		// Add two hexagon quads.
		uint prev_index_count= atomicAdd(command.indexCount, c_indices_per_quad * 2);
		uint quad_index= prev_index_count / 6;

		{
			Quad quad;

			quad.vertices[0].pos= i16vec4(int16_t(base_x), int16_t(base_y), int16_t(z), 0.0);
			quad.vertices[1].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y), int16_t(z), 0.0);
			quad.vertices[2].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z), 0.0);
			quad.vertices[3].pos= i16vec4(int16_t(base_x - 1), int16_t(base_y + 1), int16_t(z), 0.0);

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

			quad.vertices[0].pos= i16vec4(int16_t(base_x - 1), int16_t(base_y + 1), int16_t(z), 0.0);
			quad.vertices[1].pos= i16vec4(int16_t(base_x + 3), int16_t(base_y + 1), int16_t(z), 0.0);
			quad.vertices[2].pos= i16vec4(int16_t(base_x + 2), int16_t(base_y + 2), int16_t(z), 0.0);
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

			quads[quad_index + 1]= quad;
		}
	}

	int north_z= GetBlockZ(block_global_x, block_global_y + 1);
	if(z < north_z)
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
