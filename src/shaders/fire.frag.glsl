#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/dithering.glsl"
#include "inc/world_common.glsl"
#include "inc/world_shader_uniforms.glsl"

layout(push_constant) uniform push_uniforms_block
{
	float tex_shift;
};

layout(binding= 0) uniform uniforms_block
{
	WorldShaderUniforms uniforms;
};

layout(binding= 1) uniform sampler2D texture_image;

layout(location= 0) in vec2 f_tex_coord;
layout(location= 1) in flat float f_fire_power;

layout(location = 0) out vec4 out_color;

void main()
{
	vec2 tex_coord_shifted= f_tex_coord;
	tex_coord_shifted.y+= tex_shift;

	vec4 tex_value= HexagonFetch(texture_image, tex_coord_shifted);

	float y= f_tex_coord.y * 4.0;

	float inv_temperature= mix(tex_value.r, y, 0.4);
	inv_temperature= 1.0 - (1.0 - inv_temperature) * (0.6 + 0.4 * f_fire_power);

	const float c_step_width= 0.05;
	const float c_step_end_pos= 0.62;
	float alpha= 1.0 - smoothstep(c_step_end_pos - c_step_width, c_step_end_pos, inv_temperature);
	alpha*= 1.0 - smoothstep(0.8, 1.0, y);

	if(AlphaDither(alpha))
		discard;

	vec3 color_for_temperature= mix(vec3(0.5, 0.1, 0.05), vec3(1.5, 1.4, 1.2), sqrt(inv_temperature));

	out_color= vec4(color_for_temperature, alpha);
}
