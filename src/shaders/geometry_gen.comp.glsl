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

layout(binding= 0, std430) buffer vertices_buffer
{
	Quad quads[];
};

void main()
{
	uvec3 invocation= gl_GlobalInvocationID;

	float z= float(invocation.x + invocation.y) * 0.25f;
	uint quad_index= invocation.x + invocation.y * 16;

	Quad quad;

	quad.vertices[0].pos.x= float(invocation.x);
	quad.vertices[0].pos.y= float(invocation.y);
	quad.vertices[0].pos.z= z;
	quad.vertices[0].color[0]= uint8_t((invocation.x + 1) * 32);
	quad.vertices[0].color[1]= uint8_t((invocation.y + 1) * 32);
	quad.vertices[0].color[2]= uint8_t(0);

	quad.vertices[1].pos.x= float(invocation.x + 1);
	quad.vertices[1].pos.y= float(invocation.y);
	quad.vertices[1].pos.z= z;
	quad.vertices[1].color[0]= uint8_t(invocation.x * 32);
	quad.vertices[1].color[1]= uint8_t((invocation.y + 1) * 32);
	quad.vertices[1].color[2]= uint8_t(0);

	quad.vertices[2].pos.x= float(invocation.x + 1);
	quad.vertices[2].pos.y= float(invocation.y + 1);
	quad.vertices[2].pos.z= z;
	quad.vertices[2].color[0]= uint8_t(invocation.x * 32);
	quad.vertices[2].color[1]= uint8_t(invocation.y * 32);
	quad.vertices[2].color[2]= uint8_t(0);

	quad.vertices[3].pos.x= float(invocation.x);
	quad.vertices[3].pos.y= float(invocation.y + 1);
	quad.vertices[3].pos.z= z;
	quad.vertices[3].color[0]= uint8_t((invocation.x + 1) * 32);
	quad.vertices[3].color[1]= uint8_t(invocation.y * 32);
	quad.vertices[3].color[2]= uint8_t(0);

	quads[quad_index]= quad;
}
