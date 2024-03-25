#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/world_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block
{
	WorldShaderUniforms uniforms;
};

layout(binding= 1) uniform sampler2D texture_image;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color= vec4(1.0, 0.0, 1.0, 1.0);
}
