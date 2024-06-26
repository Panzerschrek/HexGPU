#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/sky_shader_uniforms.glsl"

layout(constant_id = 0) const float point_size = 1.0;

layout(binding= 0) uniform uniforms_block_variable
{
	SkyShaderUniforms uniforms;
};

layout(location=0) in vec3 pos;
layout(location=1) in vec4 color;

layout(location=0) out vec4 f_color;

void main()
{
	f_color= color;
	gl_PointSize= point_size;
	gl_Position= uniforms.stars_matrix * vec4(pos, 1.0);
}
