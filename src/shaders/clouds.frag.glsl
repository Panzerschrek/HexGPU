#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/sky_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block_variable
{
	SkyShaderUniforms uniforms;
};

layout(location= 0) in vec3 f_view_vec;

layout(location=  0) out vec4 out_color;


void main()
{
	vec3 view_vec_normalized= normalize(f_view_vec);

	vec2 cloud_coord= view_vec_normalized.xy / (abs(view_vec_normalized.z) + 0.05);

	out_color= vec4(fract(cloud_coord), 0.0, 0.5);
}
