#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/sky_shader_uniforms.glsl"
#include "inc/world_common.glsl"

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

	vec2 tc= view_vec_normalized.xy / (abs(view_vec_normalized.z + 0.2) + 0.05);

	const vec2 tc_scale= vec2(c_space_scale_x, 1.0) / 8.0;

	vec4 tex_value= HexagonFetch(texture_image, tc * tc_scale);

	out_color= vec4(tex_value.rgb, 0.5);
}
