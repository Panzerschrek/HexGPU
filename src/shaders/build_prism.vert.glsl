#version 450

layout(push_constant) uniform uniforms_block
{
	mat4 view_matrix;
};

layout(location=0) in vec3 pos;

void main()
{
	gl_Position= view_matrix * vec4(pos, 1.0);
}
