#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/sky_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block_variable
{
	SkyShaderUniforms uniforms;
};

layout(location=0) in vec4 f_color;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color= vec4(f_color.rgb, uniforms.stars_brightness);
}
