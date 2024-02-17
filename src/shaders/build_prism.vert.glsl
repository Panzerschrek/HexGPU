#version 450

layout(push_constant) uniform uniforms_block
{
	mat4 view_matrix;
};

layout(binding= 0) uniform uniforms_block_variable
{
	ivec4 build_pos;
};

layout(location=0) in vec3 pos;

void main()
{
	gl_Position= view_matrix * vec4(pos + vec3(build_pos.xyz) * vec3(3.0, 2.0, 1.0), 1.0);
}
