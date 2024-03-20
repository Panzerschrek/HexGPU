#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/sky_shader_uniforms.glsl"
#include "inc/world_common.glsl"

layout(push_constant) uniform uniforms_block
{
	vec2 tex_coord_shift;
};

layout(binding= 0) uniform uniforms_block_variable
{
	SkyShaderUniforms uniforms;
};

layout(binding= 1) uniform sampler2D texture_image;

layout(location= 0) in vec3 f_view_vec;

layout(location= 0) out vec4 out_color;

void main()
{
	vec3 view_vec_normalized= normalize(f_view_vec);

	const float horizon_level= 0.2;
	float z_adjusted= view_vec_normalized.z + horizon_level;
	vec2 tc= view_vec_normalized.xy / (abs(z_adjusted) + 0.05);

	float horizon_fade_factor= smoothstep(0.0, horizon_level, z_adjusted);

	const vec2 tc_scale= vec2(c_space_scale_x, 1.0) / 16.0;

	float tex_value= HexagonFetch(texture_image, tc * tc_scale + tex_coord_shift).r;

	float clouds_edge= uniforms.clouds_color.a;

	float clouds_brightness= tex_value * 0.2 + 0.8;

	float alpha= smoothstep(clouds_edge - 0.1, clouds_edge + 0.1, tex_value);

	out_color=
		vec4(uniforms.clouds_color.rgb * clouds_brightness,
		alpha * horizon_fade_factor);
}
