#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/world_common.glsl"
#include "inc/world_rendering_constants.glsl"
#include "inc/world_shader_uniforms.glsl"

layout(push_constant) uniform push_uniforms_block
{
	float water_phase;
};

layout(binding= 0) uniform uniforms_block
{
	WorldShaderUniforms uniforms;
};

layout(binding= 1) uniform sampler2D texture_image;

layout(location= 0) in vec2 f_light;
layout(location= 1) in vec2 f_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	vec2 tc= f_tex_coord + sin(f_tex_coord.yx * 8.0 + vec2(water_phase)) * 0.06125;

	vec4 tex_value= HexagonFetch(texture_image, tc);

	if(tex_value.a < 0.5)
		discard;

	vec3 l= CombineLight(f_light.x * c_fire_light_color, f_light.y * uniforms.sky_light_color.rgb, c_ambient_light_color);
	out_color= vec4(l * tex_value.rgb, 0.5);
}
