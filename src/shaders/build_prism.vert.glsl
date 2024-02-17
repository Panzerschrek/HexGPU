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
	vec3 pos_corrected= pos + vec3(build_pos.xyz) * vec3(3.0, 2.0, 1.0);
	pos_corrected.y+= float((build_pos.x ^ 1) & 1);
	gl_Position= view_matrix * vec4(pos_corrected, 1.0);
}
