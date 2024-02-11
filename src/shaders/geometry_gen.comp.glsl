#version 450
//#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

struct WorldVertex
{
	vec4 pos;
	vec4 reserved;
	//uint8_t color[4];
	//uint8_t reserved[16];
};

struct Quad
{
	WorldVertex vertices[4];
};

layout(binding= 0, std430) buffer vertices_buffer
{
	Quad quads[];
};

void main()
{
	uvec3 invocation= gl_GlobalInvocationID;

	float z= float(invocation.x + invocation.y) * 0.25f;
	uint quad_index= invocation.x + invocation.y * 16;

	quads[quad_index].vertices[0].pos.x= float(invocation.x);
	quads[quad_index].vertices[0].pos.y= float(invocation.y);
	quads[quad_index].vertices[0].pos.z= z;

	quads[quad_index].vertices[1].pos.x= float(invocation.x + 1);
	quads[quad_index].vertices[1].pos.y= float(invocation.y);
	quads[quad_index].vertices[1].pos.z= z;

	quads[quad_index].vertices[2].pos.x= float(invocation.x + 1);
	quads[quad_index].vertices[2].pos.y= float(invocation.y + 1);
	quads[quad_index].vertices[2].pos.z= z;

	quads[quad_index].vertices[3].pos.x= float(invocation.x);
	quads[quad_index].vertices[3].pos.y= float(invocation.y + 1);
	quads[quad_index].vertices[3].pos.z= z;
}
