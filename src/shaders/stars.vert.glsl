#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/sky_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block_variable
{
	SkyShaderUniforms uniforms;
};

layout(location=0) in vec4 pos;

void main()
{
	gl_Position= uniforms.view_matrix * vec4(pos.xyz, 1.0);
}
