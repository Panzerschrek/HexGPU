#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

struct WorldVertex
{
	vec4 pos;
	uint8_t color[4];
	uint8_t reserved[12];
};

struct Quad
{
	WorldVertex vertices[4];
};

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
	Quad quads[];
};

layout(binding= 1, std430) buffer draw_indirect_buffer
{
	VkDrawIndexedIndirectCommand command;
};

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

	int base_x= 3 * block_global_x;
	int base_y= 2 * block_global_y - (block_global_x&1) + 1;

	{
		// Add two hexagon quads.
		uint prev_index_count= atomicAdd(command.indexCount, 12);
		uint quad_index= prev_index_count / 6;

		{
			Quad quad;

			quad.vertices[0].pos.x= float(base_x);
			quad.vertices[0].pos.y= float(base_y);
			quad.vertices[0].pos.z= float(z);

			quad.vertices[1].pos.x= float(base_x + 2);
			quad.vertices[1].pos.y= float(base_y);
			quad.vertices[1].pos.z= float(z);

			quad.vertices[2].pos.x= float(base_x + 3);
			quad.vertices[2].pos.y= float(base_y + 1);
			quad.vertices[2].pos.z= float(z);

			quad.vertices[3].pos.x= float(base_x - 1);
			quad.vertices[3].pos.y= float(base_y + 1);
			quad.vertices[3].pos.z= float(z);

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

			quad.vertices[0].pos.x= float(base_x - 1);
			quad.vertices[0].pos.y= float(base_y + 1);
			quad.vertices[0].pos.z= float(z);

			quad.vertices[1].pos.x= float(base_x + 3);
			quad.vertices[1].pos.y= float(base_y + 1);
			quad.vertices[1].pos.z= float(z);

			quad.vertices[2].pos.x= float(base_x + 2);
			quad.vertices[2].pos.y= float(base_y + 2);
			quad.vertices[2].pos.z= float(z);

			quad.vertices[3].pos.x= float(base_x);
			quad.vertices[3].pos.y= float(base_y + 2);
			quad.vertices[3].pos.z= float(z);

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
		uint prev_index_count= atomicAdd(command.indexCount, 6);
		uint quad_index= prev_index_count / 6;

		Quad quad;

		quad.vertices[0].pos.x= float(base_x);
		quad.vertices[0].pos.y= float(base_y + 2);
		quad.vertices[0].pos.z= float(z);

		quad.vertices[1].pos.x= float(base_x);
		quad.vertices[1].pos.y= float(base_y + 2);
		quad.vertices[1].pos.z= float(z + 1);

		quad.vertices[2].pos.x= float(base_x + 2);
		quad.vertices[2].pos.y= float(base_y + 2);
		quad.vertices[2].pos.z= float(z + 1);

		quad.vertices[3].pos.x= float(base_x + 2);
		quad.vertices[3].pos.y= float(base_y + 2);
		quad.vertices[3].pos.z= float(z);

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
